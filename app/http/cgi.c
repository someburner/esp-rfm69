/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Israel Lot <me@israellot.com> and Jeroen Domburg <jeroen@spritesmods.com>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If we meet some day, and you think this stuff is
 * worth it, you can buy us a beer in return.
 * ----------------------------------------------------------------------------
 */
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "queue.h"

#include "user_config.h"

#include "http.h"
#include "http_parser.h"
#include "http_server.h"
#include "http_process.h"
#include "http_helper.h"

#include "flash_fs.h"
#include "cgi.h"

#ifndef CGI_DEBUG
#define CGI_DEBUG 1
#endif

#define BUILT_IN_IP "192.168.4.1"

// If a request to transmit data overflows the send buffer, the cgi function will be temporarely
// replaced by this one and later restored when all data is sent.
int cgi_transmit(http_connection *connData)
{
	CGI_DBG("cgi_transmit\n");
	struct cgi_transmit_arg *arg = (struct cgi_transmit_arg*)connData->cgi.data;

	if (arg->len > 0)
	{
		CGI_DBG("cgi_transmit %d bytes\n",arg->len);
		int rem = connData->output.buffer + HTTP_BUFFER_SIZE - connData->output.bufferPos;
		int bytesToWrite = rem;
		if(arg->len < rem ) { bytesToWrite = arg->len; }

		http_nwrite(connData,arg->dataPos,bytesToWrite);

		arg->len -= bytesToWrite;
		arg->dataPos+=bytesToWrite;
	}

	//all written
	if(arg->len==0)
	{
		//free data
		os_free(arg->data);

		//copy old cgi back
		memcpy(&connData->cgi,&arg->previous_cgi,sizeof(cgi_struct));

		//free cgi arg
		os_free(arg);
	}
	return HTTPD_CGI_MORE;
}


// This makes sure we aren't serving static files on POST requests for example
int cgi_enforce_method(http_connection *connData)
{
	enum http_method *method = (enum http_method *)connData->cgi.argument;

	if(connData->state == HTTPD_STATE_BODY_END)
   	if(connData->parser.method!=*method && (int)*method>=0)
   	{
   		HTTP_CGI_DBG("Wrong HTTP method. Enforce is %d and request is %d\n",method,connData->parser.method);

   		http_response_BAD_REQUEST(connData);
   		return HTTPD_CGI_DONE;
   	}
	return HTTPD_CGI_NEXT_RULE;
}

// This makes sure we have a body
int cgi_enforce_body(http_connection *connData)
{
	if(connData->state ==HTTPD_STATE_ON_URL)
		{ http_set_save_body(connData); }//request body to be saved

	//wait for whole body
	if(connData->state <HTTPD_STATE_BODY_END)
	{
      HTTP_CGI_DBG("cgi_enforce: next_rule\n");
		return HTTPD_CGI_NEXT_RULE;
	}

	//if body empty, bad request
	if(connData->body.len <=0)
	{
		http_response_BAD_REQUEST(connData);
      HTTP_CGI_DBG("No body\n");
		return HTTPD_CGI_DONE;
	}
	else
		{ return HTTPD_CGI_NEXT_RULE; }
}

//cgi that adds CORS ( Cross Origin Resource Sharing ) to our server
int cgi_cors(http_connection *connData)
{
	http_server_config *config = (http_server_config*)connData->cgi.argument;

	if(config==NULL)
		return HTTPD_CGI_NEXT_RULE;

	if(!config->enable_cors)
		return HTTPD_CGI_NEXT_RULE;


	if(connData->state==HTTPD_STATE_ON_URL)
	{
		//save cors headers
		http_set_save_header(connData,HTTP_ACCESS_CONTROL_REQUEST_HEADERS);
		http_set_save_header(connData,HTTP_ACCESS_CONTROL_REQUEST_METHOD);
		return HTTPD_CGI_NEXT_RULE;
	}

	if(connData->state==HTTPD_STATE_HEADERS_END)
	{
		//SET CORS Allow Origin for every request
		http_SET_HEADER(connData,HTTP_ACCESS_CONTROL_ALLOW_ORIGIN,"*");

		header * allow_headers = http_get_header(connData,HTTP_ACCESS_CONTROL_REQUEST_HEADERS);
		header * allow_methods = http_get_header(connData,HTTP_ACCESS_CONTROL_REQUEST_METHOD);

		if(allow_headers!=NULL)
			http_SET_HEADER(connData,HTTP_ACCESS_CONTROL_ALLOW_HEADERS,allow_headers->value);
		if(allow_methods!=NULL)
			http_SET_HEADER(connData,HTTP_ACCESS_CONTROL_ALLOW_METHODS,allow_methods->value);

		// Browsers will send an OPTIONS pre-flight request when posting data on a cross-domain situation
		// If that's the case here, we can safely return 200 OK with our CORS headers set
		if(connData->parser.method==HTTP_OPTIONS)
		{
			http_response_OK(connData);
			return HTTPD_CGI_DONE;
		}
	}
	return HTTPD_CGI_NEXT_RULE;
}

//Simple static url rewriter, allows us to process the request as another url without redirecting the user
//Used to serve index files on root / requests for example
int cgi_url_rewrite(http_connection *connData)
{
	if (connData->state==HTTPD_STATE_HEADERS_END)
	{
      HTTP_CGI_DBG("Rewrite %s to %s\n",connData->url,(char*)connData->cgi.argument);

		int urlSize = strlen((char*)connData->cgi.argument);
		if (urlSize < URL_MAX_SIZE)
		{
			strcpy(connData->url,(char*)connData->cgi.argument);
			//re-parse url
			http_parse_url(connData);
		}
	}
	return HTTPD_CGI_NEXT_RULE;
}

//Simple cgi that redirects the user
int cgi_redirect(http_connection *connData)
{
	http_response_REDIRECT(connData, (char*)connData->cgi.argument);
	return HTTPD_CGI_DONE;
}

//Cgi that check the request has the correct HOST header
//Using it we can ensure our server has a domain of our choice
int cgi_check_host(http_connection *connData)
{
	http_server_config *config = (http_server_config*)connData->cgi.argument;
	if(config==NULL)
		return HTTPD_CGI_NEXT_RULE;

	if(config->host_domain==NULL)
		return HTTPD_CGI_NEXT_RULE;


	if(connData->state==HTTPD_STATE_ON_URL)
	{
		http_set_save_header(connData,HTTP_HOST);
		return HTTPD_CGI_NEXT_RULE;
	}

	if(connData->state==HTTPD_STATE_HEADERS_END)
	{
		header *hostHeader = http_get_header(connData,HTTP_HOST);
		if(hostHeader==NULL)
		{
         NODE_ERR("Host header not found\n");
			http_response_BAD_REQUEST(connData);
			return HTTPD_CGI_DONE;
		}
		const char * domain = config->host_domain;

      HTTP_CGI_DBG("Host header: %s, domain: %s\n",hostHeader->value,domain);

		if(strncmp(hostHeader->value,domain,strlen(domain))==0) //compare ignoring http:// and last /
		{
         HTTP_CGI_DBG("Domain match\n");
			return HTTPD_CGI_NEXT_RULE;
		}
		else{
			uint8_t op = wifi_get_opmode();
			char ipaddrstr[17];
			os_bzero(ipaddrstr, sizeof(ipaddrstr));
			struct ip_info ipConfig;
			switch (op)
			{
				case STATIONAP_MODE:
				{
					wifi_get_ip_info(SOFTAP_IF,&ipConfig); //0x01
					ipaddr_ntoa_r(&ipConfig.ip,ipaddrstr, sizeof(ipaddrstr));

					if(strncmp(hostHeader->value,ipaddrstr,strlen(ipaddrstr))==0)
						{ HTTP_CGI_DBG("SoftAp ip match"); return HTTPD_CGI_NEXT_RULE; }
				}
				case STATION_MODE:
				{
					os_bzero(ipaddrstr, sizeof(ipaddrstr));
					wifi_get_ip_info(STATION_IF,&ipConfig); //0x00
					ipaddr_ntoa_r(&ipConfig.ip,ipaddrstr, sizeof(ipaddrstr));

					if(strncmp(hostHeader->value,ipaddrstr,strlen(ipaddrstr))==0)
						{ HTTP_CGI_DBG("Station ip match"); return HTTPD_CGI_NEXT_RULE; }
				}
			}
         HTTP_CGI_DBG("Hosts don't match\n");

			if(config->enable_captive)
			{
				//to enable a captive portal we should redirect here
				char * redirectUrl = (char *)os_zalloc(strlen(domain)+9); // domain length + http:// + / + \0
				strcpy(redirectUrl,"http://");
				os_strcat(redirectUrl,domain);
				os_strcat(redirectUrl,"/");
				http_response_REDIRECT(connData, redirectUrl);
				os_free(redirectUrl);
            HTTP_CGI_DBG("Redirect URL = %s\n", redirectUrl);

			} else {
			//bad request else
			http_response_BAD_REQUEST(connData);
			}
		return HTTPD_CGI_DONE;
		}
	}
	return HTTPD_CGI_NEXT_RULE;
}

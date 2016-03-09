#ifndef __MQTT_HELPER_H__
#define __MQTT_HELPER_H__

#define MAX_BRIDGE_STRLEN		124 //includes stubs for samples, events, meta

#define MAX_SAMPLE_STRLEN		82    //exluding anything after value colon
#define MAX_EVENT_STRLEN		80    //exluding anything after data colon
#define MAX_META_STRLEN			79    //exluding anything after data colon


/* MQTT Pub/Sub topics
 * Pubs: All are /frames. Server will know from device_id key
 * Subs: Each attached device subs to /frames/%id%
 */
#define MQTT_PUBSUB_PREFIX 		"/frames"

/* COMMON KEY STRINGS
 * Strings used for both API -> Device and Device -> API
*/
#define KEYSTR_DATETIME 		"ts"				// epoch timestamp
#define KEYSTR_DEVICE_ID     	"device_id"		// unique id

/* Keys inside various arrays */
#define KEYSTR_DATA				"data"
#define KEYSTR_SAMPLE_VALUE	"value"


#endif // __MQTT_HELPER_H__

#include "common.h"
#include "http_body.h"
#include "http_req.h"
#include "webser.h"
#include "mailsender.h"

static status webapi_test_feedid( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;
	
	webapi_resp_string( webser, "46b6adde-4317-4514-ab46-53ca1b38c44a" );
	webapi_resp_mimetype( webser, ".html" );
	return 200;
}

static status webapi_test_token( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;

	webapi_resp_string( webser, "{\"access_token\":\"eyJhbGciOiJSUzI1NiIsImtpZCI6IkQ1MTgwODlGQ0MwNTExRjIxREIyMTU5MzYwMDNDNUE0Mjc1MkNDRDIiLCJ0eXAiOiJhdCtqd3QiLCJ4NXQiOiIxUmdJbjh3RkVmSWRzaFdUWUFQRnBDZFN6TkkifQ.eyJuYmYiOjE2MzU0ODczMDAsImV4cCI6MTYzNTQ5MDkwMCwiaXNzIjoiaHR0cDovL3FhMDItdXNlci5vbGFjYW1lcmEuY29tIiwiYXVkIjpbIlVzZXJzTm90aWZpY2F0aW9ucy5hcGkiLCJhbGVydF9ub3RpZmljYXRpb25zLmFwaSIsImFsZXJ0cy5hcGkiLCJhcmVhX29mX2ludGVyZXN0LmFwaSIsImNsaXBzX3NhdmluZy5hcGkiLCJmYWxzZS1wb3NpdGl2ZXMuYXBpIiwiZmVlZF9yZWNlaXZlci5hcGkiLCJmZWVkX3N0cmVhbWluZy5hcGkiLCJmZWVkcy5hcGkiLCJncm91cHMuYXBpIiwicGVybWlzc2lvbnMuYXBpIiwicHJvZmlsZXMuYXBpIiwic2VhcmNoZXMtY29udmVuaWVuY2UuYXBpIiwidGh1bWJuYWlscy5hcGkiLCJ1c2Vycy5hcGkiXSwiY2xpZW50X2lkIjoicWEwMl9yby5jbGllbnQiLCJzdWIiOiIwYWJlYTgzMC0yOWRkLTQ5ZDAtYWM0YS0xOGU4OTM5ODE5YTMiLCJhdXRoX3RpbWUiOjE2MzU0ODczMDAsImlkcCI6ImxvY2FsIiwic2NvcGUiOlsiVXNlcnNOb3RpZmljYXRpb25zLmFwaSIsImFsZXJ0X25vdGlmaWNhdGlvbnMuYXBpIiwiYWxlcnRzLmFwaSIsImFyZWFfb2ZfaW50ZXJlc3QuYXBpIiwiY2xpcHNfc2F2aW5nLmFwaSIsImZhbHNlLXBvc2l0aXZlcy5hcGkiLCJmZWVkX3JlY2VpdmVyLmFwaSIsImZlZWRfc3RyZWFtaW5nLmFwaSIsImZlZWRzLmFwaSIsImdyb3Vwcy5hcGkiLCJwZXJtaXNzaW9ucy5hcGkiLCJwcm9maWxlcy5hcGkiLCJzZWFyY2hlcy1jb252ZW5pZW5jZS5hcGkiLCJ0aHVtYm5haWxzLmFwaSIsInVzZXJzLmFwaSIsIm9mZmxpbmVfYWNjZXNzIl0sImFtciI6WyJwd2QiXX0.RGjcDQJ3Jj82s1wHJ2mr3a2EC6uZnXpvrxqjF4gW8B5KfMpqw-k0Ld-fY5r4-hmMMCciTt509jMV4rpNPe2UruyooNELy1a5rHuu0tT2TnpEgtm9RYEPtVYKX-s9VGphIXMorCHA21Mflt3xYLmd8hUAe6cqeJ2KRwRiHVGnyp27278T_L98ivVxy4ehYb_-fuSBy2L4zDPdeHuAup44Oby2O3MzKQkRTpv-6zn7Bh_XXNiuDigeUl2aGdGRWcwxR_LQtMACtqIqz-P3blkHLtsJM7400hTnxRH8pHtRtDMbIoAxcZEdofarK1okG7lIbLu5aTizlKbVZmAbHxAXDA\",\"expires_in\":3600,\"token_type\":\"Bearer\",\"refresh_token\":\"ZXoks65t5CIPfzru340GdnYf01sdwtRtEEwzBQrZqWA\",\"scope\":\"UsersNotifications.api alert_notifications.api alerts.api area_of_interest.api clips_saving.api false-positives.api feed_receiver.api feed_streaming.api feeds.api groups.api offline_access permissions.api profiles.api searches-convenience.api thumbnails.api users.api\",\"user_id\":\"0abea830-29dd-49d0-ac4a-18e8939819a3\",\"roleClaims\":[]}" );
	webapi_resp_mimetype( webser, ".html" );
	return 200;
}


static status webapi_ota_start( event_t * ev)
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;
   
	webapi_resp_string( webser, "{\"url\":\"http://192.168.1.244/123.bin\",\"version\":3}" );
	webapi_resp_mimetype( webser, ".json" );
	return 200;
}

static status webapi_upload_record( event_t * ev )
{
	connection_t * c = ev->data;
    webser_t * webser = c->data;

	debug("req body len [%d]\n", webser->http_req_body->body_length );
	webapi_resp_string( webser, "helloworld" );
	webapi_resp_mimetype( webser, ".html" );
	return 200;
}

static status webapi_hello_world( event_t * ev )
{
    connection_t * c = ev->data;
    webser_t * webser = c->data;
  
    webapi_resp_string( webser, "helloworld" );
	webapi_resp_mimetype( webser, ".html" );
    return 200;
}

status webapi_init(  )
{
    serv_api_register( "/helloworld", webapi_hello_world );
	serv_api_register( "/connect/token", webapi_test_token );
	serv_api_register( "/api/FeedsManager/AddFeed", webapi_test_feedid );
	serv_api_register( "/api/FeedReceiver/UploadChunk", webapi_hello_world );
	serv_api_register( "/api/VideoAnalytics/ProcessMotionData", webapi_hello_world );
	serv_api_register( "/api/ota", webapi_ota_start );
	serv_api_register( "/upload/birdtest/12345.mp4", webapi_upload_record );
	return OK;
}


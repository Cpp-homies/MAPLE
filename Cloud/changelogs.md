## Change logs
### 05/04
**App to Cloud interaction**
- For PUT and UPDATE request: added soil humidity "soil_humid", the old "humid" key in the json will be change to "air_humid"
- For GET request: added soil humidity "soil_humidity", and change the old "humid" key into "air_humidity"
- Sensor data resource now located at "\<base url\>/sensordata/\<string:user_id\>/\<string:timestamp\>" instead of "\<base url\>/sensordata/\<string:timestamp\>/" in order to distinguish data entries from different users and their devices. All requests that previously went to the mentioned URL should now be redirected to the new one.
- The "\<baseurl\>/datatable" resource is now moved to "\<baseurl\>/datatable/\<user_id:string\>" to distinguish between the users. All requests that previously went to the mentioned URL should now be redirected to the new one.

**Device to Cloud interaction**
- The "\<baseurl\>/datatable" resource is now moved to "\<baseurl\>/datatable/\<user_id:string\>" to distinguish between the users
- The "\<baseurl\>/request-to-esp" resource is now moved to "\<baseurl\>/request-to-esp/\<user_id:string\>" to distinguish between the users. All requests that previously went to the mentioned URL should now be redirected to the new one.

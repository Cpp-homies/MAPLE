## Change logs
### 07/04
**App to Cloud interaction**
- A new user_id and password hash pair can be add to the database by sending PUT to "\<base url\>/auth/register" with two parameters "user_id" and "password"
- A hashed user_id and password pair can be check by sending GET to "\<base url\>/auth/login" with two parameters "user_id" and "password". This will have the following return values
	- If the username is not found: 404 HTTP response code
	- If the username is found: a dictionary with "status" attributes:
		- `response["status"] == 0`: password does not match the one in the database, authentication failed
		- `response["status"] == 1`: password matches the one in the database, authentication was successful
- User's password can be change using PATCH requets to "\<base url\>/auth/login" with two parameters "user_id" and "password".
- A session must be authenticate by logging in first before any request can be made.

### 05/04
**App to Cloud interaction**
- For PUT and UPDATE request: added soil humidity "soil_humid", the old "humid" key in the json will be change to "air_humid"
- For GET request: added soil humidity "soil_humidity", and change the old "humid" key into "air_humidity"
- Sensor data resource now located at "\<base url\>/sensordata/\<string:user_id\>/\<string:timestamp\>" instead of "\<base url\>/sensordata/\<string:timestamp\>/" in order to distinguish data entries from different users and their devices. All requests that previously went to the mentioned URL should now be redirected to the new one.
- The "\<baseurl\>/datatable" resource is now moved to "\<baseurl\>/datatable/\<user_id:string\>" to distinguish between the users. All requests that previously went to the mentioned URL should now be redirected to the new one.

**Device to Cloud interaction**
- The "\<baseurl\>/datatable" resource is now moved to "\<baseurl\>/datatable/\<user_id:string\>" to distinguish between the users
- The "\<baseurl\>/request-to-esp" resource is now moved to "\<baseurl\>/request-to-esp/\<user_id:string\>" to distinguish between the users. All requests that previously went to the mentioned URL should now be redirected to the new one.
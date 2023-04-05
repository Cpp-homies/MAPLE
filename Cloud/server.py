from flask import Flask, jsonify
# from celery import Celery
from flask_restful import Api, Resource, reqparse, abort, marshal_with, fields
from flask_sqlalchemy import SQLAlchemy
from enum import Enum
import time
import asyncio

# Define HOST and PORT
HOST = '0.0.0.0'
PORT = 8001

# An enum to use as a shorthand for the request codes for ESP requests
class ESP_Req_Code(Enum):
    NO_REQ = 0
    LIVE_DATA = 1


app = Flask(__name__)
api = Api(app)
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///sensor_data.db'
db = SQLAlchemy(app)

# create the schema for the student
class SensorDataModel(db.Model):
    timestamp = db.Column(db.String(20), primary_key=True)
    user_id = db.Column(db.String, primary_key=True)
    temperature = db.Column(db.Float, nullable=False)
    air_humidity = db.Column(db.Float, nullable=False)
    soil_humidity = db.Column(db.Float, nullable=False)

    def __repr__(self):
        return f"Data(time={self.timestamp}, user={self.user_id}, temp={self.temperature}, air_humid={self.air_humidity}, soil_humid={self.soil_humidity})"
    
# db.create_all()

# argument parser for put
sensor_put_args = reqparse.RequestParser()
sensor_put_args.add_argument('temp', type=str, help='Temperature not specified', required=True)
sensor_put_args.add_argument('air_humid', type=str, help='Air humidity not specified', required=True)
sensor_put_args.add_argument('soil_humid', type=str, help='Soil humidity not specified', required=True)

# argument parser for "live data" put
sensor_live_put_args = reqparse.RequestParser()
sensor_live_put_args.add_argument('time', type=str, help='Timestamp not specified', required=True)
# sensor_live_put_args.add_argument('user_id', type=str, help='User ID not specified', required=True)
sensor_live_put_args.add_argument('temp', type=str, help='Temperature not specified', required=True)
sensor_live_put_args.add_argument('air_humid', type=str, help='Air humidity not specified', required=True)
sensor_live_put_args.add_argument('soil_humid', type=str, help='Soil humidity not specified', required=True)

# argument parser for update
sensor_update_args = reqparse.RequestParser()
sensor_update_args.add_argument('temp', type=str, help='Temperature not specified', required=False)
sensor_update_args.add_argument('air_humid', type=str, help='Humidity not specified', required=False)
sensor_update_args.add_argument('soil_humid', type=str, help='Humidity not specified', required=False)


# fields for serialization
resource_fields = {
    'timestamp': fields.String,
    'user_id': fields.String,
    'temperature': fields.Float,
    'air_humidity':fields.Float,
    'soil_humidity':fields.Float
}

# Some data for sending requests to the ESP
esp_reqs = [] # A queue of requests to send to the ESP
esp_req_timeout = 10 # Maximum time to wait for the ESP response (in seconds)

# helper function that wait until a certain external condition (list type) turns true,
# or until some timeout interval has been reached
def wait_until(cond, timeout):
    start_time = time.time()
    cur_time = start_time
    while (cond == []) and (cur_time - start_time) < timeout:
        cur_time = time.time() # advance cur_time
        # do nothing

    # if timeout was reach, return False
    if (cur_time - start_time) >= esp_req_timeout:
        return False
    return True

# class Clients():
#     def __init__(self, user_id):
#         self.user_id = user_id
#         self.live_data
        

# test resource
class SensorData(Resource):
    live_data = {} # A buffer to hold live data from the device
    esp_req_served = [] # Flag to inform when the most recent request was served by the ESP

    # test get function that return a dictionary with "timestamp", "temperature", "air_humidity", and "soil_humidity"
    @marshal_with(resource_fields)
    def get(self, timestamp, user_id):
        # Check if this is a request for live data or not
        if timestamp == "live":
            # Ask the ESP to update the live data (function executed aynchronously in the background)
            update_status = asyncio.run(self.update_live_data(user_id))
            # print(update_status)
            if not update_status:
                # Inform the sender that the request has timed out
                print("[INFO] Request to ESP timeout")
                abort(408, message="Request timeout - no response received from the ESP")
            else:
                del self.esp_req_served[:] # reset the flag
                print(self.live_data)
                return self.live_data

        else:
             # Query data by user ID
            results = SensorDataModel.query.filter_by(user_id=user_id).all()

            # Check if user ID exists
            if not results:
                abort(404, message="User ID not found")

            # Filter data by timestamp
            result = next((item for item in results if item.timestamp == timestamp), None)

            # Check if timestamp exists
            if not result:
                abort(404, message="Timestamp not found")
    
    # update the live data from the ESP
    async def update_live_data(self, user_id):
         # Add the request to the pending request list for the ESP
        esp_reqs.append({"user_id": user_id, "type":ESP_Req_Code.LIVE_DATA.value})

        # Wait until the ESP respond or timeout has been reached
        req_succeed = wait_until(self.esp_req_served, esp_req_timeout)
        if not req_succeed:
            # Inform the sender that the request has timed out
            return False
        else:
            return True
        
    @marshal_with(resource_fields)
    def put(self, timestamp, user_id):
        try:
            if timestamp == "live":
                args = sensor_live_put_args.parse_args()

                # Add the data to the live_data dictionary
                self.live_data["timestamp"] = args['time']
                self.live_data["user_id"] = user_id
                self.live_data["temperature"] = float(args["temp"])
                self.live_data["air_humidity"] = float(args["air_humid"])
                self.live_data["soil_humidity"] = float(args["soil_humid"])
                
                self.esp_req_served.append(True)

                return self.live_data, 201
            
            args = sensor_put_args.parse_args()
            # check whether the timestamp is taken or not
            result = SensorDataModel.query.filter_by(timestamp=timestamp, user_id=user_id).first()
            # print("Received PUT request for timestamp " + time + " with arguments: " + "{temp:" + args['temp'] + ",humid:" \
            #       + args['humid'] + '}')
            if result:
                abort(409, message='Timestamp already exist')

            # create a SensorDataModel object and add it to the session
            data = SensorDataModel(timestamp=timestamp, user_id=user_id, temperature=float(args['temp']), air_humidity=float(args['air_humid']), soil_humidity=float(args['soil_humid']))
            db.session.add(data)
            db.session.commit()
            return data, 201
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")
    
    @marshal_with(resource_fields)
    def patch(self, timestamp, user_id):
        try:
            args = sensor_update_args.parse_args()

            # check whether the timestamp exist or not
            result = SensorDataModel.query.filter_by(timestamp=timestamp, user_id=user_id).first()
            if not result:
                abort(404, message='Timestamp not found')
            
            if args['temp']:
                result.temperature = float(args['temp'])
            if args['air_humid']:
                result.humidity = float(args['air_humid'])
            if args['soil_humid']:
                result.humidity = float(args['soil_humid'])

            db.session.commit()

            return result, 201
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")
    
    @app.route('/<string:user_id>/datatable', methods=['GET'])
    def get_data_table(user_id):
        # Query all data rows from the database
        rows = SensorDataModel.query.filter_by(user_id=user_id).all()

        # Convert the data rows into a list of dictionaries
        data_list = []
        for row in rows:
            data_dict = {
                'timestamp': row.timestamp,
                'temperature': row.temperature,
                'air_humidity': row.air_humidity,
                'soil_humidity': row.soil_humidity
            }
            data_list.append(data_dict)

        # Return the list of data rows as a JSON response
        return jsonify(data_list)
    
    # GET method to get the next request for the ESP to execute
    @app.route('/<string:user_id>/request-to-esp', methods=['GET'])
    def get_esp_request(user_id):
        if esp_reqs:
            return esp_reqs.pop()
        return {"type": ESP_Req_Code.NO_REQ.value}
        

    # @marshal_with(resource_fields)    
    # def delete(self, studentID):
    #     # check whether the student ID exist or not
    #     result = SensorDataModel.query.filter_by(id=studentID).first()
    #     if not result:
    #         abort(404, message='Student ID not found')

    #     # delete the student
    #     session = db.session()
    #     session.delete(result)
    #     session.commit()

    #     return result, 201

    
    # def post(self):
    #     return {'data':'Posted'}
 
api.add_resource(SensorData, "/sensordata/<string:user_id>/<string:timestamp>")


if __name__ == "__main__":
    app.run(host=HOST,port=PORT, threaded=True)

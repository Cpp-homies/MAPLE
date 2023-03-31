from flask import Flask, jsonify
# from celery import Celery
from flask_restful import Api, Resource, reqparse, abort, marshal_with, fields
from flask_sqlalchemy import SQLAlchemy
from enum import Enum
import time

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
    temperature = db.Column(db.Float, nullable=False)
    humidity = db.Column(db.Float, nullable=False)

    def __repr__(self):
        return f"Data(time={self.timestamp}, temp={self.temperature}, humid={self.humidity})"
    
# db.create_all()

# argument parser for put
sensor_put_args = reqparse.RequestParser()
sensor_put_args.add_argument('temp', type=str, help='Temperature not specified', required=True)
sensor_put_args.add_argument('humid', type=str, help='Humidity not specified', required=True)

# argument parser for "live data" put
sensor_live_put_args = reqparse.RequestParser()
sensor_live_put_args.add_argument('time', type=str, help='Timestamp not specified', required=True)
sensor_live_put_args.add_argument('temp', type=str, help='Temperature not specified', required=True)
sensor_live_put_args.add_argument('humid', type=str, help='Humidity not specified', required=True)

# argument parser for update
sensor_update_args = reqparse.RequestParser()
sensor_update_args.add_argument('temp', type=str, help='Temperature not specified', required=False)
sensor_update_args.add_argument('humid', type=str, help='Humidity not specified', required=False)


# fields for serialization
resource_fields = {
    'timestamp': fields.String,
    'temperature': fields.Float,
    'humidity':fields.Float
}

# Some data for sending requests to the ESP
esp_reqs = [] # A queue of requests to send to the ESP
esp_req_timeout = 10 # Maximum time to wait for the ESP response (in seconds)

# test resource
class SensorData(Resource):
    live_data = {} # A buffer to hold live data from the device
    esp_req_served = [] # Flag to inform when the most recent request was served by the ESP

    # test get function that return a dictionary with "Hello World":"Hello"
    @marshal_with(resource_fields)
    def get(self, timestamp):
        # Check if this is a request for live data or not
        if timestamp == "live":
            # Add the request to the request list for the ESP
            esp_reqs.append({"type":ESP_Req_Code.LIVE_DATA.value})

            start_time = time.time()
            cur_time = start_time
            # Wait for the ESP's response
            while (self.esp_req_served == []) and (cur_time - start_time) < esp_req_timeout:
                cur_time = time.time() # advance cur_time
                # do nothing

            
            if (cur_time - start_time) >= esp_req_timeout:
                # Inform the sender that the request has timed out
                print("[INFO] Request to ESP timeout")
                abort(408, message="Request timeout - no response received from the ESP")
            else:
                print(self.esp_req_served)
                del self.esp_req_served[:] # reset the flag
                
                print(self.live_data)
                return self.live_data


        # query and return the data
        result = SensorDataModel.query.filter_by(timestamp=timestamp).first()
        # print("Received GET request for timestamp " + time)
        if not result:
            abort(404, message="Timestamp not found")
        return result
    
    @marshal_with(resource_fields)
    def put(self, timestamp):
        try:
            if timestamp == "live":
                args = sensor_live_put_args.parse_args()

                # Add the data to the dictionary reference
                self.live_data["timestamp"] = args['time']
                self.live_data["temperature"] = float(args["temp"])
                self.live_data["humidity"] = float(args["humid"])
                
                self.esp_req_served.append(True)

                return self.live_data, 201
            
            args = sensor_put_args.parse_args()
            # check whether the timestamp is taken or not
            result = SensorDataModel.query.filter_by(timestamp=timestamp).first()
            # print("Received PUT request for timestamp " + time + " with arguments: " + "{temp:" + args['temp'] + ",humid:" \
            #       + args['humid'] + '}')
            if result:
                abort(409, message='Timestamp already exist')

            # create a SensorDataModel object and add it to the session
            data = SensorDataModel(timestamp=timestamp, temperature=float(args['temp']), humidity=float(args['humid']))
            db.session.add(data)
            db.session.commit()
            return data, 201
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")
    
    @marshal_with(resource_fields)
    def patch(self, timestamp):
        try:
            args = sensor_update_args.parse_args()

            # check whether the timestamp exist or not
            result = SensorDataModel.query.filter_by(timestamp=timestamp).first()
            if not result:
                abort(404, message='Timestamp not found')
            
            if args['temp']:
                result.temperature = float(args['temp'])
            if args['humid']:
                result.humidity = float(args['humid'])

            db.session.commit()

            return result, 201
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")
    
    @app.route('/datatable', methods=['GET'])
    def get_data_table():
        # Query all data rows from the database
        rows = SensorDataModel.query.all()

        # Convert the data rows into a list of dictionaries
        data_list = []
        for row in rows:
            data_dict = {
                'timestamp': row.timestamp,
                'temperature': row.temperature,
                'humidity': row.humidity
            }
            data_list.append(data_dict)

        # Return the list of data rows as a JSON response
        return jsonify(data_list)
    
    # GET method to get the next request for the ESP to execute
    @app.route('/request-to-esp', methods=['GET'])
    def get_esp_request():
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
 
api.add_resource(SensorData, "/sensordata/<string:timestamp>")


if __name__ == "__main__":
    app.run(host=HOST,port=PORT, threaded=True)

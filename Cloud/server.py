from flask import Flask, jsonify, session, request, send_from_directory
# from celery import Celery
from flask_restful import Api, Resource, reqparse, abort, marshal_with, marshal, fields
from flask_sqlalchemy import SQLAlchemy
from sqlalchemy.dialects.sqlite import DATETIME
from sqlalchemy import delete
from flask_cors import CORS, cross_origin
from flask_session import Session
from enum import Enum
import time
import datetime
from datetime import timedelta
from datetime import datetime
import asyncio
from werkzeug.utils import secure_filename
from PIL import Image
import os
import logging

# Set up logging to a file and console
logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s %(levelname)s: %(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S',
                    handlers=[
                        logging.FileHandler('flask.log'),
                        logging.StreamHandler()
                    ])

# Define HOST and PORT
HOST = '0.0.0.0'
PORT = 8000

# An enum to use as a shorthand for the request codes for ESP requests
class ESP_Req_Code(Enum):
    NO_REQ = 0
    LIVE_DATA = 1
    SET_PUMP_THRSHLD = 2
    SET_FAN_THRSHLD = 3
    SET_LIGHT_INTENSITY = 4
    SET_LIGHT_STARTTIME = 5
    SET_LIGHT_STOPTIME = 6


app = Flask(__name__)
api = Api(app)
SECRET_KEY = "secret"
SESSION_TYPE = 'filesystem'
app.secret_key = "secret"
app.config.from_object(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///sensor_data.db'
app.config.update(SESSION_COOKIE_SAMESITE="None", SESSION_COOKIE_SECURE=True)
app.config['PERMANENT_SESSION_LIFETIME'] =  timedelta(minutes=59)
app.config['UPLOAD_FOLDER'] = 'images_'


db = SQLAlchemy(app)
# cors = CORS(app, origins="*")
Session(app)
CORS(app, origins="*", supports_credentials=True)

# Custom timestamp format to store in the DB
custom_datetime_format = DATETIME(storage_format="%(year)04d_%(month)02d_%(day)02d_%(hour)02d_%(minute)02d_%(second)02d",
                                  regexp=r"(\d+)_(\d+)_(\d+)_(\d+)_(\d+)_(\d+)"
                                 )
# create the schema for the sensor data
class SensorDataModel(db.Model):
    timestamp = db.Column(custom_datetime_format, primary_key=True)
    user_id = db.Column(db.String, primary_key=True)
    temperature = db.Column(db.Float, nullable=False)
    air_humidity = db.Column(db.Float, nullable=False)
    soil_humidity = db.Column(db.Float, nullable=False)

    # Return a dictionary representation of the data model
    def to_dict(self):
        return {
            'timestamp': self.timestamp.strftime("%Y_%m_%d_%H_%M_%S"),
            'user_id': self.user_id,
            'temperature': self.temperature,
            'air_humidity': self.air_humidity,
            'soil_humidity': self.soil_humidity
        }

    def __repr__(self):
        return f"Data(timestamp={self.timestamp}, user_id={self.user_id}, temp={self.temperature}, air_humid={self.air_humidity}, soil_humid={self.soil_humidity})"

# Helper function that takes in an input string in the form "YYYY_mm_dd_HH_MM_SS" and convert it to the data model time
def str_to_datamodelTime(input_string):
    
    return datetime.strptime(input_string, "%Y_%m_%d_%H_%M_%S")

# Helper function that takes in an datetime object and return its string representation in the form "YYYY_mm_dd_HH_MM_SS"
def datamodelTime_to_str(datetime_obj):
    
    return datetime_obj.strftime("%Y_%m_%d_%H_%M_%S")

# Helper function that return a dictionary with the given attributes and keys
# according to the resource fields. Similar to the automatic marshal(sensor_data, resource_fields) call 
# but more customizable 
def make_sensordataDict(timestamp, user_id, temperature, air_humidity, soil_humidity):
    # 'timestamp': fields.String,
    # 'user_id': fields.String,
    # 'temperature': fields.Float,
    # 'air_humidity':fields.Float,
    # 'soil_humidity':fields.Float
    result_dict = {'timestamp': timestamp, 'user_id': user_id, \
                    'temperature': float(temperature), 'air_humidity': float(air_humidity), 'soil_humidity': float(soil_humidity)}
    
    return result_dict

# Helper function that return the current time as string with the format "%Y_%m_%d_%H_%M_%S"
def get_current_time_string():
    return datetime.now().strftime("%Y_%m_%d_%H_%M_%S")


# create the schema for the student
class UserAuthModel(db.Model):
    user_id = db.Column(db.String(20), primary_key=True)
    password = db.Column(db.String, nullable=False)

    def __repr__(self):
        return f"Auth(user_id={self.user_id}, password={self.password})"
    
# db.create_all()

# argument parser for put
sensor_put_args = reqparse.RequestParser()
sensor_put_args.add_argument('temp', type=str, help='Temperature not specified', required=True)
sensor_put_args.add_argument('air_humid', type=str, help='Air humidity not specified', required=True)
sensor_put_args.add_argument('soil_humid', type=str, help='Soil humidity not specified', required=True)

# argument parser for ESP's put
sensor_esp_put_args = reqparse.RequestParser()
sensor_esp_put_args.add_argument('user_id', type=str, help='User ID not specified', required=True)
sensor_esp_put_args.add_argument('password', type=str, help='Password not specified', required=True)
sensor_esp_put_args.add_argument('temp', type=str, help='Temperature not specified', required=True)
sensor_esp_put_args.add_argument('air_humid', type=str, help='Air humidity not specified', required=True)
sensor_esp_put_args.add_argument('soil_humid', type=str, help='Soil humidity not specified', required=True)
sensor_esp_put_args.add_argument('time', type=str, help='Timestamp not specified', required=False)

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

# argument parser for ESP's control parameter update
esp_controlParUpdate_args = reqparse.RequestParser()
esp_controlParUpdate_args.add_argument('user_id', type=str, help='User ID not specified', required=True)
esp_controlParUpdate_args.add_argument('password', type=str, help='Password not specified', required=True)
esp_controlParUpdate_args.add_argument('lightIntensity', type=int, help='Light percentage not specified', required=True)
esp_controlParUpdate_args.add_argument('fanThreshold', type=int, help='Fan threshold not specified', required=True)
esp_controlParUpdate_args.add_argument('pumpThreshold', type=int, help='Pump trigger percent not specified', required=True)
esp_controlParUpdate_args.add_argument('lightStartTime', type=str, help='Light start time not specified', required=True)
esp_controlParUpdate_args.add_argument('lightStopTime', type=str, help='Light stop time not specified', required=True)



# fields for serialization
resource_fields = {
    'timestamp': fields.String,
    'user_id': fields.String,
    'temperature': fields.Float,
    'air_humidity':fields.Float,
    'soil_humidity':fields.Float
}

# Some data for sending requests to the ESP
# esp_reqs = [] # A queue of requests to send to the ESP
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

# A class to represent a client and related temporary data to that client
class Client():
    def __init__(self, user_id, password):
        self.user_id = user_id
        self.password = password
        self.live_data = {} # the most recently updated live data for this client
        self.esp_reqs = [] # the list of request for the esp (device) of this client
        self.esp_req_served = [] # flag that indicates if the most recent request for this client has been served or not

        # device's control parameters
        self.pumpThreshold = 0
        self.fanThreshold = 0
        self.lightIntensity = 0
        self.lightStartTime = '00:00'
        self.lightStopTime = '00:00'
    
    # set the device's control parameters
    def set_esp_control_pars(self, pumpThreshold, fanThreshold, lightIntensity, lightStartTime, lightStopTime):
        self.pumpThreshold = pumpThreshold
        self.fanThreshold = fanThreshold
        self.lightIntensity = lightIntensity
        self.lightStartTime = lightStartTime
        self.lightStopTime = lightStopTime

    # string representation of the client's control parameters
    def controlPars_string(self):
        return f"Control parameters: \npump threshold={self.pumpThreshold}\nfan threshold={self.fanThreshold}\nlight intensity={self.lightIntensity}\nlight start time={self.lightStartTime}\nlight stop time={self.lightStopTime}"



    # get the most recent request from the request list, return a default no new request JSON if there's no new request
    def pop_esp_req(self):
        if self.esp_reqs:
            return self.esp_reqs.pop()
        return {"type": ESP_Req_Code.NO_REQ.value}
    
    # Add new request for this user's esp (device), the request arguments is given in the form of a dictionary
    def add_esp_req(self, req_args):
        # # make a dictionary object with this user's id and then all the key-value pairs in the req_args
        # req_dict = {'user_id': self.user_id}
        # req_dict.update(req_args)

        # Append the req_dict to the list of requests
        self.esp_reqs.append(req_args)

    # Raise the esp_req_served flag
    def raise_esp_req_served(self):
        self.esp_req_served.append(True)
    
    def lower_esp_req_served(self):
        del self.esp_req_served[:]



# helper functions for working with the client class
## find the user with the specified ID from a list
def find_client(user_list, user_id):
    # sSarch for the user object with the correct self.user_id == user_id attribute and return it
    for user in user_list:
        # print(user.user_id)
        if user.user_id == user_id:
            return user

    # If there is no user with the specified user_id, return None
    return None

    
# A list of clients (users)
clients = []

# A list of authenticated IP addresses
authIP = []

# test resource
class SensorData(Resource):

    # live_data = {} # A buffer to hold live data from the device
    # esp_req_served = [] # Flag to inform when the most recent request was served by the ESP

    # test get function that return a dictionary with "timestamp", "temperature", "air_humidity", and "soil_humidity"
    # @marshal_with(resource_fields)
    @cross_origin(supports_credentials=True)
    def get(self, user_id, timestampstr):
        #### NOTE #######
        # Currently disable authentication for faster testing, remember to uncomment this part later
        #################

        # Check if this data access is authorized (by loging in) or not
        # if not check_user_auth(user_id):
        #     return {'message': 'Unauthorized access'}, 401

        # Check if this is a request for live data or not
        if timestampstr == "live":
            client = find_client(clients, user_id)

            if client:
                # Ask the ESP to update the live data (function executed aynchronously in the background)
                update_status = asyncio.run(self.update_live_data(client))
                # print(update_status)
                if not update_status:
                    # Inform the sender that the request has timed out
                    print("[INFO] Request to ESP timeout")
                    abort(408, message="Request timeout - no response received from the ESP")
                else:
                    client.lower_esp_req_served() # reset the req_served flag
                    # print(client.live_data)
                    return client.live_data
            else:
                abort(404, message="User ID not found")

        try:
            # convert given string timestamp into datetime type
            timestamp_converted = str_to_datamodelTime(timestampstr)
             # Query data by user ID
            results = SensorDataModel.query.filter_by(user_id=user_id).all()
            # print(results)

            # Check if user ID exists
            if not results:
                abort(404, message="User ID not found")

            # Filter data by timestamp
            result = next((item for item in results if item.timestamp == timestamp_converted), None)

            # Check if timestamp exists
            if not result:
                abort(404, message="Timestamp not found")
            
            # Convert the SensorData object result into a dictionary form 
            result_dict = marshal(result, resource_fields)

            result_dict['timestamp'] = timestampstr # change the timestamp into the correct displaying format 
            return result_dict
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")
    
    # Get sensor data in a time range
    @app.route('/get-range/<string:user_id>/<string:timerange>', methods=['GET'])
    @cross_origin(supports_credentials=True)
    def get_in_range(user_id, timerange):
        #### NOTE #######
        # Currently disable authentication for faster testing, remember to uncomment this part later
        #################

        # Check if this data access is authorized (by loging in) or not
        # if not check_user_auth(user_id):
        #     return {'message': 'Unauthorized access'}, 401


        try:
            # Convert the given range (from start time to end time) into two datetime objects
            start_time_str, end_time_str = timerange.split('-')
            start_time = datetime.strptime(start_time_str, "%Y_%m_%d_%H_%M_%S")
            
            # Convert the shorthand 'now' into a time string of the current time if needed
            if end_time_str == 'now':
                end_time_str = get_current_time_string() # Get the current time as string

            print(end_time_str)
            end_time = datetime.strptime(end_time_str, "%Y_%m_%d_%H_%M_%S")

            # Query data by user ID
            results = SensorDataModel.query.filter_by(user_id=user_id).all()
            # print(results)

            # Check if user ID exists
            if not results:
                abort(404, message="User ID not found")

            # Filter data by timestamp
            result_raw = SensorDataModel.query.filter( \
                                SensorDataModel.user_id == user_id, \
                                SensorDataModel.timestamp.between(start_time, end_time) \
                            ).all()

            # Check if timestamp exists
            if not result_raw:
                abort(404, message="Timestamp not found")
            
            # Convert raw result to json
            result_json = jsonify([data.to_dict() for data in result_raw])
            # # Convert the SensorData object result into a dictionary form 
            # result_dict = marshal(result, resource_fields)

            # result_dict['timestamp'] = timestampstr # change the timestamp into the correct displaying format 
            return result_json
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")

    # update the live data from the ESP
    async def update_live_data(self, client):
         # Add the request to the pending request list for the ESP
        client.add_esp_req({"type":ESP_Req_Code.LIVE_DATA.value})

        # Wait until the ESP respond or timeout has been reached
        req_succeed = wait_until(client.esp_req_served, esp_req_timeout)
        if not req_succeed:
            # Inform the sender that the request has timed out
            return False
        else:
            return True
        
    # @marshal_with(resource_fields)
    @cross_origin(supports_credentials=True)
    def put(self, user_id, timestampstr):
        #### NOTE #######
        # Currently disable authentication for faster testing, remember to uncomment this part later
        #################

        # Check if this data access is authorized (by loging in) or not
        # if not check_user_auth(user_id):
        #     return {'message': 'Unauthorized access'}, 401
        try:
            if timestampstr == "live":
                args = sensor_live_put_args.parse_args()
                client = find_client(clients, user_id)

                if client:

                    # Add the data to the live_data dictionary
                    client.live_data["timestamp"] = args['time']
                    client.live_data["user_id"] = user_id
                    client.live_data["temperature"] = float(args["temp"])
                    client.live_data["air_humidity"] = float(args["air_humid"])
                    client.live_data["soil_humidity"] = float(args["soil_humid"])
                    
                    client.raise_esp_req_served() # raise the esp_req_served flag

                    # print(client.live_data)
                    return client.live_data, 201
                
                else:
                    abort(404, message="User ID not found")

            timestamp_converted = str_to_datamodelTime(timestampstr)
            args = sensor_put_args.parse_args()
            # check whether the timestamp is taken or not
            result = SensorDataModel.query.filter_by(timestamp=timestamp_converted, user_id=user_id).first()
            # print("Received PUT request for timestamp " + time + " with arguments: " + "{temp:" + args['temp'] + ",humid:" \
            #       + args['humid'] + '}')
            if result:
                abort(409, message='Timestamp already exist')

            # create a SensorDataModel object and add it to the session
            data = SensorDataModel(timestamp=timestamp_converted, user_id=user_id, temperature=float(args['temp']), air_humidity=float(args['air_humid']), soil_humidity=float(args['soil_humid']))
            db.session.add(data)
            db.session.commit()
            return make_sensordataDict(timestampstr, user_id, args['temp'], args['air_humid'], args['soil_humid'])
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")

    # Similar to the normal PUT request but for the ESP, and require a password argument
    # @marshal_with(resource_fields)
    @app.route('/esp-req/sensordata/<string:timestampstr>', methods=['PUT'])
    @cross_origin(supports_credentials=True)
    def esp_put(timestampstr):
        #### NOTE #######
        # Currently disable authentication for faster testing, remember to uncomment this part later
        #################
        
        # Check if this data access is authorized (by loging in) or not
        # if not check_user_auth(user_id):
        #     return {'message': 'Unauthorized access'}, 401
        try:
            args = sensor_esp_put_args.parse_args()
            user_id = args['user_id']
            # print(args)
            # print(clients)
            client = find_client(clients, user_id)
            if not client:
                abort(404, message="User ID not found")
            else:
                if client.password != args['password']:
                    return {'message': 'Unauthorized access'}, 401
                
            if timestampstr == "live":
                # Add the data to the live_data dictionary
                client.live_data["timestamp"] = args['time']
                client.live_data["user_id"] = user_id
                client.live_data["temperature"] = float(args["temp"])
                client.live_data["air_humidity"] = float(args["air_humid"])
                client.live_data["soil_humidity"] = float(args["soil_humid"])
                
                client.raise_esp_req_served() # raise the esp_req_served flag

                # print(client.live_data)
                return client.live_data, 201
            
            else:
                timestamp_converted = str_to_datamodelTime(timestampstr)

                # args = sensor_esp_put_args.parse_args()
                # check whether the timestamp is taken or not
                result = SensorDataModel.query.filter_by(timestamp=timestamp_converted, user_id=user_id).first()
                # print("Received PUT request for timestamp " + time + " with arguments: " + "{temp:" + args['temp'] + ",humid:" \
                #       + args['humid'] + '}')
                if result:
                    abort(409, message='Timestamp already exist')

                # create a SensorDataModel object and add it to the session
                data = SensorDataModel(timestamp=timestamp_converted, user_id=user_id, temperature=float(args['temp']), air_humidity=float(args['air_humid']), soil_humidity=float(args['soil_humid']))
                db.session.add(data)
                db.session.commit()
                return marshal(data, resource_fields), 201
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")
    
    @marshal_with(resource_fields)
    @cross_origin(supports_credentials=True)
    def patch(self, timestampstr, user_id):
        # Check if this data access is authorized (by loging in) or not
        if not check_user_auth(user_id):
            return {'message': 'Unauthorized access'}, 401
        try:
            args = sensor_update_args.parse_args()

            # Query data by user ID
            results = SensorDataModel.query.filter_by(user_id=user_id).all()

            # Check if user ID exists
            if not results:
                abort(404, message="User ID not found")

            # Filter data by timestamp
            result = next((item for item in results if item.timestamp == timestampstr), None)

            # Check if timestamp exists
            if not result:
                abort(404, message="Timestamp not found")
            
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
    
    @app.route('/datatable/<string:user_id>', methods=['GET'])
    @cross_origin(supports_credentials=True)
    def get_data_table(user_id):
        #### NOTE #######
        # Currently disable authentication for faster testing, remember to uncomment this part later
        #################

        # Check if this data access is authorized (by loging in) or not
        # if not check_user_auth(user_id):
        #     return {'message': 'Unauthorized access'}, 401

        ###
        # OLD implementation
        ###
        # Query all data rows from the database
        # rows = SensorDataModel.query.filter_by(user_id=user_id).all()

        # # Convert the data rows into a list of dictionaries
        # data_list = []
        # for row in rows:
        #     data_dict = {
        #         'timestamp': row.timestamp,
        #         'user_id': row.user_id,
        #         'temperature': row.temperature,
        #         'air_humidity': row.air_humidity,
        #         'soil_humidity': row.soil_humidity
        #     }
        #     data_list.append(data_dict)

        # # Return the list of data rows as a JSON response
        # return jsonify(data_list)

        # Query data by user ID
        results = SensorDataModel.query.filter_by(user_id=user_id).all()
        # print(results)

        # Check if user ID exists
        if not results:
            abort(404, message="User ID not found")
        
        # Convert raw result to json
        result_json = jsonify([data.to_dict() for data in results])
        # # Convert the SensorData object result into a dictionary form 
        # result_dict = marshal(result, resource_fields)

        # result_dict['timestamp'] = timestampstr # change the timestamp into the correct displaying format 
        return result_json
    
    # GET method to get the next request for the ESP to execute
    @app.route('/request-to-esp/<string:user_id>/<string:password>', methods=['GET'])
    @cross_origin(supports_credentials=True)
    def get_esp_request(user_id, password):
        # # Check if this data access is authorized (by loging in) or not
        # auth_user = get_ip_auth_user(request.remote_addr, authIP)
        
        # if not check_user_auth(user_id):
        #     return {'message': 'Unauthorized access'}, 401
        
        client = find_client(clients, user_id)
        if not client:
            abort(404, message="User ID not found")
        else:
            if client.password == password:
                return client.pop_esp_req()
            else:
                return {'message': 'Unauthorized access'}, 401
        # else:
        #     abort(404, message="User ID not found")
        

    @app.route('/delete-all/<string:user_id>', methods=['DELETE'])
    @cross_origin(supports_credentials=True)
    def delete_all(user_id): 
        # Check if this data access is authorized (by loging in) or not
        if not check_user_auth(user_id):
            return {'message': 'Unauthorized access'}, 401
        try:
            
            db.session.query(SensorDataModel).filter(SensorDataModel.user_id == user_id).delete()
            db.session.commit()
            return {'status': 1}, 200
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")

    
    # def post(self):
    #     return {'data':'Posted'}
 



########################
# Authentication code
########################
# Helper functions

# Function that checks if the user is authenticated or not
def check_user_auth(user_id):
    print("Passed user_id:", user_id)
    print("Session user_id:", session.get('auth_user'))
    print(session.sid)
    if user_id == session.get('auth_user'):
        return True
    return False

# Checks if a given IP is authenticated or not, returns the user that the IP is authenticated for
def get_ip_auth_user(ip_address, ip_user_list):
    # Check if the IP address is in the user_list dictionary
    if ip_address in ip_user_list:
        return ip_user_list[ip_address]
    else:
        return None

# argument parser for put
login_put_args = reqparse.RequestParser()
login_put_args.add_argument('user_id', type=str, help='Username not specified', required=True)
login_put_args.add_argument('password', type=str, help='Password not specified', required=True)

# login_patch_args = reqparse.RequestParser()
# login_patch_args.add_argument('password', type=str, help='Password not specified', required=True)


# User authenication fields for serialization
userAuth_fields = {
    'user_id': fields.String,
    'password': fields.String,
}
class Authentication(Resource):
    @app.route('/auth/login', methods=['GET'])
    @cross_origin(supports_credentials=True)
    def get_login():
        try:
            args = login_put_args.parse_args()

            # check whether the username exist or not
            result = UserAuthModel.query.filter_by(user_id=args["user_id"]).first()

            if not result:
                return {'message': 'Username does not exist'}, 404

            if (result.password == args['password']):
                # Remember that this session has been authenticated for the given user ID
                session["auth_user"] = args['user_id']

                # Add a new Client() object for this user if it didn't exist yet
                user_id = args['user_id']
                if not find_client(clients, user_id):
                    clients.append(Client(user_id, args['password']))
                    # print(clients)

                # # Add a new IP - User pair to the list of authenticated IP address if it does not exist yet
                # ip_address = request.remote_addr
                # if not get_ip_auth_user(ip_address, authIP):
                #     authIP.append({ip_address: user_id})
                #     print(authIP)

                return {'status': 1}, 201
            return {'status': 0}, 201
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")
    

    @app.route('/auth/login', methods=['PATCH'])
    @cross_origin(supports_credentials=True)
    def patch_login():
        try:
            args = login_put_args.parse_args()

            # check whether the username exist or not
            result = UserAuthModel.query.filter_by(user_id=args["user_id"]).first()

            if not result:
                return {'message': 'Username does not exist'}, 404

            if args['password']:
                result.password = args['password']

            db.session.commit()
            return marshal(result, userAuth_fields), 201
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")




        
    @app.route('/auth/register', methods=['PUT'])
    @cross_origin(supports_credentials=True)
    # @marshal_with(userAuth_fields)
    def put_register():
        try:
            
            args = login_put_args.parse_args()

            # check whether the username is taken or not
            result = UserAuthModel.query.filter_by(user_id=args["user_id"]).first()

            if result:
                return {'message': 'Username already exist'}, 409

            # create a SensorDataModel object and add it to the session
            data = UserAuthModel(user_id=args["user_id"], password=args["password"])
            db.session.add(data)
            db.session.commit()
            return marshal(data, userAuth_fields), 201
        except ValueError as e:
            # Catch and handle ValueError exceptions
            abort(400, message="Invalid arguments")

api.add_resource(SensorData, "/sensordata/<string:user_id>/<string:timestampstr>")
api.add_resource(SensorData, '/get-range/<string:user_id>/<string:timerange>', methods=['GET'], endpoint="get-range")
api.add_resource(SensorData, "/delete-all/<string:user_id>", methods=['DELETE'], endpoint="delete-all")
api.add_resource(Authentication, "/auth")
api.add_resource(Authentication, "/auth/register", endpoint="register")
api.add_resource(Authentication, "/auth/login", endpoint="login")

@app.route('/', methods=['GET'])
def get_server_online_status():
    return {'status': 1}, 200

################
# Image sending/receiving
################
# POST Request for uploading image to the server
@app.route('/upload-image/<user_id>', methods=['POST'])
def upload_image(user_id):
    file = request.files['file']
    if file.filename == '':
        return "No selected file", 400
    if file:
        filename = secure_filename(file.filename)
        
        user_folder_path = os.path.join(app.config['UPLOAD_FOLDER'], user_id)
        
        if not os.path.exists(user_folder_path):
            os.makedirs(user_folder_path)
        
        img_path = os.path.join(user_folder_path, filename)
        file.save(img_path)

        # Open an image file
        with Image.open(img_path) as img:
            # Resize it
            img.thumbnail((800, 600))

            # Change file extension to .webp
            base_filename, file_extension = os.path.splitext(filename)
            new_filename = base_filename + ".webp"

            # Save as webp
            new_img_path = os.path.join(user_folder_path, new_filename)
            img.save(new_img_path, "WEBP")

        # Remove the original file
        os.remove(img_path)
        return 'File successfully uploaded'

# GET Request for accessing images from the server
@app.route('/images/<user_id>/<filename>', methods=['GET'])
def get_image(user_id, filename):
    try: 
        user_folder_path = os.path.join(app.config['UPLOAD_FOLDER'], user_id)
    
        # Check if the directory exists
        if not os.path.exists(user_folder_path):
            return jsonify({"error": "User not found"}), 404
    
        # Get the most recent image
        if filename == 'latest':
            # Get list of all files in user's directory
            image_list = os.listdir(user_folder_path)

            # Filter out non-image files if needed
            image_list = [filename for filename in image_list if filename.endswith(('.png', '.jpg', '.jpeg', 'webp'))]

            # Sort the image list
            image_list.sort()

            lastest_image = image_list[-1]

            return send_from_directory(user_folder_path, lastest_image)
        
        return send_from_directory(user_folder_path, filename)
    except FileNotFoundError:
        abort(404)

# GET Request that return a list of images in the user's directory
@app.route('/image-list/<user_id>', methods=['GET'])
def get_imageList(user_id):
    user_folder_path = os.path.join(app.config['UPLOAD_FOLDER'], user_id)
    
    # Check if the directory exists
    if not os.path.exists(user_folder_path):
        return jsonify({"error": "User not found"}), 404

    # Get list of all files in user's directory
    image_list = os.listdir(user_folder_path)

    # Filter out non-image files if needed
    image_list = [filename for filename in image_list if filename.endswith(('.png', '.jpg', '.jpeg', '.webp'))]

    # Sort the image list
    image_list.sort()

    # Create a dictionary where the keys are the indices and the values are the filenames
    image_dict = {i+1: filename for i, filename in enumerate(image_list)}

    return jsonify(image_dict)

################
################

##################
# Remote control #
##################
## GET methods ##
@app.route('/pumpThreshold/get', methods=['OPTIONS', 'GET'])
@cross_origin(supports_credentials=True)
def get_pumpThreshold():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'GET':
        # Handle GET request
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        pumpThreshold = client.pumpThreshold

        return jsonify(pumpThreshold), 200


@app.route('/fanThreshold/get', methods=['OPTIONS', 'GET'])
@cross_origin(supports_credentials=True)
def get_fanThreshold():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'GET':
        # Handle GET request
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        fanThreshold = client.fanThreshold

        return jsonify(fanThreshold), 200


@app.route('/lightIntensity/get', methods=['OPTIONS', 'GET'])
@cross_origin(supports_credentials=True)
def get_lightIntensity():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'GET':
        # Handle GET request
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        lightIntensity = client.lightIntensity

        return jsonify(lightIntensity), 200


@app.route('/lightStartTime/get', methods=['OPTIONS', 'GET'])
@cross_origin(supports_credentials=True)
def get_lightStartTime():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'GET':
        # Handle GET request
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        lightStartTime = client.lightStartTime

        return jsonify(lightStartTime), 200


@app.route('/lightStopTime/get', methods=['OPTIONS', 'GET'])
@cross_origin(supports_credentials=True)
def get_lightStopTime():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'GET':
        # Handle GET request
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        lightStopTime = client.lightStopTime

        return jsonify(lightStopTime), 200

    

## SET methods ##
@app.route('/pumpThreshold/set', methods=['OPTIONS', 'PUT'])
@cross_origin(supports_credentials=True)
def set_pumpThreshold():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'PUT':
        # Handle PUT request
        data = request.get_json()
        value = data['value']
        print(value)
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        client.add_esp_req({"type":ESP_Req_Code.SET_PUMP_THRSHLD.value, "value":value})

        return {'message':"Request added successfully"}, 200


@app.route('/fanThreshold/set', methods=['OPTIONS', 'PUT'])
@cross_origin(supports_credentials=True)
def set_fanThreshold():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'PUT':
        # Handle PUT request
        data = request.get_json()
        value = data['value']
        print(value)
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        client.add_esp_req({"type":ESP_Req_Code.SET_FAN_THRSHLD.value, "value":value})

        return {'message':"Request added successfully"}, 200
    

@app.route('/lightIntensity/set', methods=['OPTIONS', 'PUT'])
@cross_origin(supports_credentials=True)
def set_lightIntensity():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'PUT':
        # Handle PUT request
        data = request.get_json()
        value = data['value']
        print(value)
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        client.add_esp_req({"type":ESP_Req_Code.SET_LIGHT_INTENSITY.value, "value":value})

        return {'message':"Request added successfully"}, 200
    

@app.route('/lightStartTime/set', methods=['OPTIONS', 'PUT'])
@cross_origin(supports_credentials=True)
def set_lightStartTime():
    user_id = session.get('auth_user')

    # if not user_id:
    #     abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'PUT':
        # Handle PUT request
        data = request.get_json()
        value = data['value']
        print(value)
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        client.add_esp_req({"type":ESP_Req_Code.SET_LIGHT_STARTTIME.value, "value":value})

        return {'message':"Request added successfully"}, 200
    

@app.route('/lightStopTime/set', methods=['OPTIONS', 'PUT'])
@cross_origin(supports_credentials=True)
def set_lightStopTime():
    user_id = session.get('auth_user')

    if not user_id:
        abort(404, message="No authenticated user (check if you are logged-in)")

    if request.method == 'OPTIONS':
        # Respond to OPTIONS request
        return "Received preflight request", 200

    if request.method == 'PUT':
        # Handle PUT request
        data = request.get_json()
        value = data['value']
        print(value)
        client = find_client(clients, user_id)

        if not client:
            abort(404, message="Client not found")

        client.add_esp_req({"type":ESP_Req_Code.SET_LIGHT_STOPTIME.value, "value":value})

        return {'message':"Request added successfully"}, 200


# Used by the ESP to update the control parameters stored on the server
# With the up-to-date ones on the ESP
@app.route('/esp-control-pars-update', methods=['PUT'])
@cross_origin(supports_credentials=True)
def esp_control_par_update():
    try:
        args = esp_controlParUpdate_args.parse_args()
        user_id = args['user_id']
        client = find_client(clients, user_id)
        if not client:
            abort(404, message="User ID not found")
        else:
            if client.password != args['password']:
                return {'message': 'Unauthorized access'}, 401
            
        # Set the control parameters using the set_esp_control_pars method
        client.set_esp_control_pars(
            pumpThreshold=float(args['pumpThreshold']),
            fanThreshold=float(args['fanThreshold']),
            lightIntensity=float(args['lightIntensity']),
            lightStartTime=args['lightStartTime'],
            lightStopTime=args['lightStopTime']
        )
        # print(client.controlPars_string())

        return "Server's control parameters updated successfully", 201
        
    except ValueError as e:
        # Catch and handle ValueError exceptions
        abort(400, message="Invalid arguments")



if __name__ == "__main__":
    app.run(host=HOST,port=PORT, threaded=True)

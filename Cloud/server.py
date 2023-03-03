from flask import Flask
from flask_restful import Api, Resource, reqparse, abort, marshal_with, fields
from flask_sqlalchemy import SQLAlchemy


app = Flask(__name__)
api = Api(app)
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///sensor_data.db'
db = SQLAlchemy(app)

# create the schema for the student
class SensorDataModel(db.Model):
    timestamp = db.Column(db.String(20), primary_key=True)
    temperature = db.Column(db.Integer, nullable=False)
    humidity = db.Column(db.Integer, nullable=False)

    def __repr__(self):
        return f"Data(time={self.timestamp}, temp={self.temperature}, humid={self.humidity})"
    
db.create_all()

# argument parser for put
sensor_put_args = reqparse.RequestParser()
sensor_put_args.add_argument('temp', type=str, help='Temperature not specified', required=True)
sensor_put_args.add_argument('humid', type=str, help='Humidity not specified', required=True)


# fields for serialization
resource_fields = {
    'timestamp': fields.String,
    'temperature': fields.String,
    'humidity':fields.Integer
}

# test resource
class SensorData(Resource):
    # test get function that return a dictionary with "Hello World":"Hello"
    @marshal_with(resource_fields)
    def get(self, time):
        # query and return the data
        result = SensorDataModel.query.filter_by(timestamp=time).first()
        if not result:
            abort(404, message="Timestamp not found")
        return result
    
    @marshal_with(resource_fields)
    def put(self, time):
        args = sensor_put_args.parse_args()
        
        # check whether the student ID is taken or not
        result = SensorDataModel.query.filter_by(timestamp=time).first()
        if result:
            abort(409, message='Timestamp already exist.')

        # create a student object and add it to the session
        data = SensorDataModel(timestamp=time, temperature=args['temp'], humidity=args['humid'])
        db.session.add(data)
        db.session.commit()
        print('received request')
        return data, 201
    
    # @marshal_with(resource_fields)
    # def patch(self, studentID):
    #     args = sensor_put_args.parse_args()

    #     # check whether the student ID exist or not
    #     result = SensorDataModel.query.filter_by(id=studentID).first()
    #     if not result:
    #         abort(404, message='Student ID not found')
        
    #     if args['name']:
    #         result.name = args['name']
    #     if args['age']:
    #         result.age = args['age']

    #     db.session.commit()

    #     return result, 201

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
 
api.add_resource(SensorData, "/sensordata/<string:time>")


if __name__ == "__main__":
    app.run(debug=True, host='0.0.0.0')
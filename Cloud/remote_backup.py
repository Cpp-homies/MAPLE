import os
import pathlib
import mimetypes
import datetime
from google.cloud import storage # pip install google-cloud-storage

STORAGE_CLASSES = ('STANDARD', 'NEARLINE', 'COLDLINE', 'ARCHIVE')
os.environ['GOOGLE_APPLICATION_CREDENTIALS'] = 'Keys/sunlit-vortex-383512-87d7f5a8052e.json'

class GCStorage:
    def __init__(self, storage_client):
        self.client = storage_client

    def create_bucket(self, bucket_name, storage_class, bucket_location='US'):
        bucket = self.client.bucket(bucket_name)
        bucket.storage_class = storage_class
        return self.client.create_bucket(bucket, bucket_location)        

    def get_bucket(self, bucket_name):
        return self.client.get_bucket(bucket_name)

    def list_buckets(self):
        buckets = self.client.list_buckets()
        return [bucket.name for bucket in buckets]

    def upload_file(self, bucket, blob_destination, file_path):
        # file_type = file_path.split('.')[-1]
        # if file_type == 'csv':
        #     content_type = 'text/csv'
        # elif file_type == 'psd':
        #     content_type = 'image/vnd.adobe.photoshop'
        # else:

        print(f"Uploading {file_path} to GCS bucket at {blob_destination}")
        content_type = mimetypes.guess_type(file_path)[0]
        blob = bucket.blob(blob_destination)
        blob.upload_from_filename(file_path, content_type=content_type)
        return blob

    def list_blobs(self, bucket_name):
        return self.client.list_blobs(bucket_name)
        
# Helper function to get the current date and time in the format "YYYY-MM-DD-HH-MM-SS"
def get_current_datetime():
    now = datetime.datetime.now()
    return now.strftime("%Y-%m-%d-%H-%M-%S")

# Helper function that backup the file to the specified bucket
# Takes in a GCS object, destination bucket name, 
# the filename of the backup (just the root name, without appending the "_backup-timestamp" formating), 
# and the local directory of the file
#
# Returns a status code indicating if the backup succeeded or not, 1 == succeed, 0 == failed
def back_up_file(gcs, bucket_name, backup_filename, local_filedir):
    # Create gcp_api_demo Cloud Storage bucket if it doesn't exist already
    if not bucket_name in gcs.list_buckets():
        bucket_gcs = gcs.create_bucket(bucket_name, STORAGE_CLASSES[0])
    else:
        bucket_gcs = gcs.get_bucket(bucket_name)

    # Upload backup files
    # for file_path in files_folder.glob('*.*'):

    # use full file name

    # Construct backup file name based on current date and time
    file_type = local_filedir.split('.')[-1]
    
    complete_backup_filename = backup_filename + "_backup_" + get_current_datetime() + "." + file_type

    # Upload the file to Google Cloud Storage
    gcs.upload_file(bucket_gcs, complete_backup_filename, local_filedir)
    
# Prepare the variables
working_dir = pathlib.Path.cwd()
files_folder = working_dir #.joinpath('My Files')
downloads_folder = working_dir.joinpath('Downloaded')
bucket_name = 'maple_backups'

# Construct GCStorage instance
storage_client = storage.Client()
gcs = GCStorage(storage_client)


# Backup the data
back_up_file(gcs, bucket_name, 'sensor_data', str(files_folder) + "/sensor_data.db")








# Download & Delete Files
# gcs_demo_blobs = gcs.list_blobs('gcs_api_demo')
# for blob in gcs_demo_blobs:
#     path_download = downloads_folder.joinpath(blob.name)
#     if not path_download.parent.exists():
#         path_download.parent.mkdir(parents=True)
#     blob.download_to_filename(str(path_download))
#     blob.delete()
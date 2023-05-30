let hashedUserId = localStorage.getItem('user_id');
let latestImageUrl = `https://cloud.kovanen.io/images/${hashedUserId}/latest`;
var img = document.getElementById('latestImage');

export function updatePhoto() {
    hashedUserId = localStorage.getItem('user_id');
    let latestImageUrl = `https://cloud.kovanen.io/images/${hashedUserId}/latest`;
    // update the timestamp to the URL to ensure the browser doesn't use a cached image
    document.getElementById("latestImage").src = latestImageUrl + "?t=" + new Date().getTime();
}

// Update image every 3 seconds
setInterval(updatePhoto, 3000);

// Get the modal
var modal = document.getElementById("modal");

// Get the image and insert it inside the modal
var img = document.getElementById("latestImage");
var modalImg = document.getElementById("img01");

img.onclick = function(){
  modal.style.display = "block";
  modalImg.src = this.src;
}

// Get the <span> element that closes the modal
var span = document.getElementById("closeModal");

// When the user clicks on <span> (x), close the modal
span.onclick = function(event) {
  modal.style.display = "none";
}

// Also close the modal if the user clicks anywhere outside of the image in the modal
window.onclick = function(event) {
  if (event.target == modal) {
    modal.style.display = "none";
  }
}

// Set the onload function
img.onload = function() {
  // When the image has loaded, display the photoFeed block
  document.getElementById('photoFeedBlock').style.display = 'inline-block';
};

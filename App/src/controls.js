var lightIntensitySlider = document.getElementById("lightIntensity");
var lightIntensityOutput = document.getElementById("lightIntensityValue");
var fanThresholdSlider = document.getElementById("fanThreshold");
var fanThresholdOutput = document.getElementById("fanThresholdValue");
var pumpThresholdSlider = document.getElementById("pumpThreshold");
var pumpThresholdOutput = document.getElementById("pumpThresholdValue");
var lightStartTimeInput = document.getElementById("lightStartTime");
var lightStartTimeOutput = document.getElementById("lightStartTimeValue");
var lightStopTimeInput = document.getElementById("lightStopTime");
var lightStopTimeOutput = document.getElementById("lightStopTimeValue");

const url = "https://cloud.kovanen.io/";

export async function startControl() {
    // Get the data from the server
    let fetchedLightIntensity = await fetchFromServer("lightIntensity");
    let fetchedFanThreshold = await fetchFromServer("fanThreshold");
    let fetchedPumpThreshold = await fetchFromServer("pumpThreshold");
    let fetchedLightStartTime = await fetchFromServer("lightStartTime");
    let fetchedLightStopTime = await fetchFromServer("lightStopTime");

    // Set the values
    if (fetchedLightIntensity != null) {
        lightIntensityOutput.innerHTML = fetchedLightIntensity;
    } else if (localStorage.getItem("lightIntensity")) {
        lightIntensityOutput.innerHTML = localStorage.getItem("lightIntensity");
    }

    if (fetchedFanThreshold != null) {
        fanThresholdOutput.innerHTML = fetchedFanThreshold;
    } else if (localStorage.getItem("fanThreshold")) {
        fanThresholdOutput.innerHTML = localStorage.getItem("fanThreshold");
    }

    if (fetchedPumpThreshold != null) {
        pumpThresholdOutput.innerHTML = fetchedPumpThreshold;
    } else if (localStorage.getItem("pumpThreshold")) {
        pumpThresholdOutput.innerHTML = localStorage.getItem("pumpThreshold");
    }

    if (fetchedLightStartTime != null) {
        lightStartTimeOutput.innerHTML = fetchedLightStartTime;
    } else if (localStorage.getItem("lightStartTime")) {
        lightStartTimeOutput.innerHTML = localStorage.getItem("lightStartTime");
    }

    if (fetchedLightStopTime != null) {
        lightStopTimeOutput.innerHTML = fetchedLightStopTime;
    } else if (localStorage.getItem("lightStopTime")) {
        lightStopTimeOutput.innerHTML = localStorage.getItem("lightStopTime");
    }

}

// Update the current light intensity slider value (when you stop dragging the slider handle)
lightIntensitySlider.onchange = function () {
    lightIntensityOutput.innerHTML = this.value;

    send("lightIntensity", this.value);

    localStorage.setItem("lightIntensity", this.value);
}

// Update the current fan threshold slider value (when you stop dragging the slider handle)
fanThresholdSlider.onchange = function () {
    fanThresholdOutput.innerHTML = this.value;

    send("fanThreshold", this.value);

    localStorage.setItem("fanThreshold", this.value);
}

// Update the current pump threshold slider value (when you stop dragging the slider handle)
pumpThresholdSlider.onchange = function () {
    pumpThresholdOutput.innerHTML = this.value;

    send("pumpThreshold", this.value);

    localStorage.setItem("pumpThreshold", this.value);
}

// Update the current light start time input value (when you enter a new value)
lightStartTimeInput.onchange = function () {
    lightStartTimeOutput.innerHTML = this.value;

    send("lightStartTime", this.value);

    localStorage.setItem("lightStartTime", this.value);
}

// Update the current light stop time input value (when you enter a new value)
lightStopTimeInput.onchange = function () {
    lightStopTimeOutput.innerHTML = this.value;

    send("lightStopTime", this.value);
    
    localStorage.setItem("lightStopTime", this.value);
}


// Send the data to the server
async function send(what, value) {
    const setUrl = `${url}${what}/set`;

    // Create the request body data
    const data = value;

    const status = document.getElementById(`${what}Status`);
    status.innerHTML = "Loading...";
    status.style.color = "black";

    // Make the request
    await fetch(setUrl, {
        method: 'PUT',
        headers: {
            'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
    })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            console.log('Success:', data);
            status.innerHTML = "OK";
            status.style.color = "green";
            return data;
        })
        .catch((error) => {
            console.error('Error:', error);
            status.innerHTML = "Error";
            status.style.color = "red";
            return null;
        });
}

// Fetch the light intensity from the server
async function fetchFromServer(what) {

    const fetchUrl = `${url}${what}/get`;
    const status = document.getElementById(`${what}Status`);
    status.innerHTML = "Loading...";
    status.style.color = "black";

    // Make the request
    await fetch(fetchUrl, {
        method: 'GET',
    })
        .then(response => {
            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }
            return response.json();
        })
        .then(data => {
            console.log('Success:', data);
            status.innerHTML = "OK";
            status.style.color = "green";
            return data;
        })
        .catch((error) => {
            console.error('Error:', error);
            status.innerHTML = "Error";
            status.style.color = "red";
            return null;
        });
}
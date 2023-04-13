import Chart from 'chart.js/auto';
import 'chartjs-adapter-luxon';
import zoomPlugin from 'chartjs-plugin-zoom';
import { login_fetch } from './auth.js';

Chart.register(zoomPlugin);

// Set colors from the CSS variables
const cssVariables = getComputedStyle(document.documentElement);

const lightColor1 = cssVariables.getPropertyValue('--LightColor1').trim();
const lightColor2 = cssVariables.getPropertyValue('--LightColor2').trim();
const mediumLightColor1 = cssVariables.getPropertyValue('--MediumLightColor1').trim();
const mediumColor1 = cssVariables.getPropertyValue('--MediumColor1').trim();
const mediumDarkColor1 = cssVariables.getPropertyValue('--MediumDarkColor1').trim();
const mediumDarkColor2 = cssVariables.getPropertyValue('--MediumDarkColor2').trim();
const darkColor1 = cssVariables.getPropertyValue('--DarkColor1').trim();
const darkColor2 = cssVariables.getPropertyValue('--DarkColor2').trim();
const darkColor2T = cssVariables.getPropertyValue('--DarkColor2T').trim();
const highLightColor1 = cssVariables.getPropertyValue('--HighLightColor1').trim();
const highLightColor2 = cssVariables.getPropertyValue('--HighLightColor2').trim();


let userId = localStorage.getItem('user_id');
let userId_text = localStorage.getItem('user_id_text');
let token = localStorage.getItem('token');

let URL = `https://cloud.kovanen.io/datatable/${userId}`;

let interval;

// Get login info from local storage
function getLoginInfo() {
    userId = localStorage.getItem('user_id');
    userId_text = localStorage.getItem('user_id_text');
    token = localStorage.getItem('token');
    URL = `https://cloud.kovanen.io/datatable/${userId}`;
}

// Login if user ID and token are found in local storage
if (userId && token) {
    login_fetch(userId, token, userId_text);
}

// Initialize the temperature chart
const ctx = document.getElementById('temperatureChart').getContext('2d');
const chartData = {
    labels: [],
    datasets: [
        {
            label: 'Temperature',
            data: [],
            borderColor: highLightColor1,
            borderWidth: 5,
            fill: false,
            tension: 0.4,
            yAxisID: 'temperature',
            pointRadius: 0,
        },
        {
            label: 'Humidity',
            data: [],
            borderColor: highLightColor2,
            borderWidth: 5,
            fill: false,
            tension: 0.4,
            yAxisID: 'humidity',
            pointRadius: 0,
        },
    ],
};

const chartConfig = {
    type: 'line',
    data: chartData,
    options: {
        responsive: true,
        maintainAspectRatio: false,
        interaction: {
            intersect: false,
            mode: 'index',
        },
        scales: {
            x: {
                type: 'time',
                bounds: 'data',
                min: new Date().getTime() - 2 * 60 * 60 * 1000, // 2 hours ago
                // max: new Date().getTime(), // Set the maximum x-value to the current time
                time: {
                    unit: 'hour',
                    displayFormats: {
                        hour: 'MMM D, HH:mm',
                    },
                },
                grid: {
                    color: darkColor1,
                },
                ticks: {
                    color: lightColor2,
                },
            },
            temperature: {
                type: 'linear',
                position: 'left',
                suggestedMin: 0,
                suggestedMax: 50,
                grid: {
                    color: darkColor1,
                },
                ticks: {
                    color: lightColor2,
                },
            },
            humidity: {
                type: 'linear',
                position: 'right',
                suggestedMin: 0,
                suggestedMax: 100,
                grid: {
                    color: darkColor1,
                },
                ticks: {
                    color: lightColor2,
                },
            },
        },
        plugins: {
            title: {
                display: true,
            },
            zoom: {
                pan: {
                    enabled: true,
                    mode: 'x',
                },
                zoom: {
                    wheel: {
                        enabled: true,
                    },
                    pinch: {
                        enabled: true,
                    },
                    mode: 'x',
                },
                limits: {
                    x: {
                        max: Date.now(),
                    },
                },
            },
        },
    },
};




const temperatureChart = new Chart(ctx, chartConfig);

function updateTemperatureChart(timestamp, temperature, humidity) {
    chartData.labels.push(timestamp);
    chartData.datasets[0].data.push(temperature);
    chartData.datasets[1].data.push(humidity);
    temperatureChart.update();
}

// Fetch data from the server
async function fetchdata() {
    // Log the user ID to the
    console.log('User ID: ' + userId);
    if (!userId) {
        console.error('No user ID found. Please log in or register first.');
        return;
    }
    await fetch(URL, { credentials: 'include' })
        .then(response => response.json())
        .then(data => {
            console.log(data);
            try {
                // reformat the timestamps from YYYY_MM_DD_HH_MM_SS to seconds
                // first filter out data that is not in correct format
                data = data.filter(item => {
                    return item.timestamp.split('_').length === 6;
                });
                // then reformat the timestamps
                data.forEach((item, index) => {
                    const date = item.timestamp.split('_');
                    const year = date[0];
                    const month = date[1] - 1;
                    const day = date[2];
                    const hour = date[3];
                    const minute = date[4];
                    const second = date[5];
                    const timestamp = new Date(Date.UTC(year, month, day, hour, minute, second));
                    data[index].timestamp = timestamp.getTime();
                });
                // sort the data by timestamp
                data.sort((a, b) => {
                    return -(a.timestamp - b.timestamp);
                });
                document.getElementById('air-humidity').innerHTML = Math.floor(data[0].air_humidity);
                document.getElementById('air-temp').innerHTML = Math.floor(data[0].temperature);
                document.getElementById('soil-humidity').innerHTML = Math.floor(data[0].soil_humidity);
                // soil humidity
                console.log(data[0].soil_humidity);
                document.getElementById('timestamp').innerHTML = new Date(data[0].timestamp).toLocaleString();


                // Update the chart with the most recent data
                const lastTimestamp = chartData.labels[chartData.labels.length - 1];
                const currentTime = Date.now();
                const firstTimestamp = currentTime - 2 * 24 * 60 * 60 * 1000; // 2 days ago

                // If the chart is not empty, only add new data
                if (lastTimestamp !== undefined) {
                    const newData = data.filter(item => {
                        return item.timestamp < lastTimestamp && item.timestamp > firstTimestamp;
                    });
                    newData.forEach((item) => {
                        updateTemperatureChart(item.timestamp, item.temperature, item.air_humidity);
                    });
                } else {
                    const recentData = data.filter(item => item.timestamp > firstTimestamp);
                    recentData.forEach((item) => {
                        updateTemperatureChart(item.timestamp, item.temperature, item.air_humidity);
                    });
                }

            } catch (e) {
                console.log(e);
            }
        });
}


// Initiate the loading popup
async function active_fetchdata() {
    if (userId && token) {
        document.getElementById('loading-popup').style.display = 'flex';
        await fetchdata();
        document.getElementById('loading-popup').style.display = 'none';
    } else {
        console.error('No user ID found. Please log in or register first.');
    }
}

// Zoom reset button
document.getElementById('resetZoom').addEventListener('click', () => {
    temperatureChart.resetZoom();
});

// Fetch data button
document.getElementById('requestData').addEventListener('click', () => {
    active_fetchdata();
});

// Start interval
export function startInterval() {
    getLoginInfo();
    active_fetchdata();
    interval = setInterval(fetchdata, 10000);
}

// Stop interval
export function stopInterval() {
    clearInterval(interval);
}

// Clear the data
export function clearData() {
    chartData.labels = [];
    chartData.datasets[0].data = [];
    chartData.datasets[1].data = [];
    temperatureChart.update();
}

// Start interval if the user is logged in
if (userId !== null) {
    startInterval();
}
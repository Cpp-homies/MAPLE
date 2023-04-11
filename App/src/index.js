import Chart from 'chart.js/auto';
import 'chartjs-adapter-luxon';
import zoomPlugin from 'chartjs-plugin-zoom';

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



const URL = 'https://cloud.kovanen.io/datatable'

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
        interaction: {
          intersect: false,
          mode: 'index',
        },
        scales: {
            x: {
                type: 'time',
                bounds: 'data',
                min: new Date().getTime() - 24 * 60 * 60 * 1000,
                time : {
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
async function fetchdata(){
    await fetch(URL)
        .then(response => response.json())
        .then(data => {
            try{
                // reformat the timestamps from YYYY_MM_DD_HH_MM_SS to seconds
                // first filter out data that is not in correct format
                data = data.filter(item => {
                    return item.timestamp.split('_').length === 6;
                });
                // then reformat the timestamps
                data.forEach((item, index) => {
                    const date = item.timestamp.split('_');
                    const year = date[0];
                    const month = date[1]-1;
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
                document.getElementById('air-humidity').innerHTML = Math.floor(data[0].humidity);
                document.getElementById('air-temp').innerHTML = Math.floor(data[0].temperature);
                document.getElementById('timestamp').innerHTML = new Date(data[0].timestamp).toLocaleString();
                

                // Update the chart with the most recent data
                const lastTimestamp = chartData.labels[chartData.labels.length - 1];
                const currentTime = Date.now();
                const lastweek = currentTime - 7 * 24 * 60 * 60 * 1000;

                // If the chart is not empty, only add new data
                if (lastTimestamp !== undefined) {
                    const newData = data.filter(item => {
                        return item.timestamp < lastTimestamp && item.timestamp > lastweek;
                    });
                    newData.forEach((item) => {
                        updateTemperatureChart(item.timestamp, item.temperature, item.humidity);
                    });
                } else {
                    const recentData = data.filter(item => item.timestamp > lastweek);
                    recentData.forEach((item) => {
                        updateTemperatureChart(item.timestamp, item.temperature, item.humidity);
                    });
                }
                
            } catch (e) {
                console.log(e);
            }
        });
}


// Initiate the loading popup
async function active_fetchdata(){
    document.getElementById('loading-popup').style.display = 'flex';
    await fetchdata();
    document.getElementById('loading-popup').style.display = 'none';
}

// Zoom reset button
document.getElementById('resetZoom').addEventListener('click', () => {
    temperatureChart.resetZoom();
});

// Fetch data button
document.getElementById('requestData').addEventListener('click', () => {
    active_fetchdata();
});

active_fetchdata();
const interval = setInterval(fetchdata, 10000);
const URL = 'https://cloud.kovanen.io/datatable'

// Initialize the temperature chart
const ctx = document.getElementById('temperatureChart').getContext('2d');
const chartData = {
    labels: [],
    datasets: [
        {
        label: 'Temperature',
        data: [],
        borderColor: 'rgba(255, 99, 71, 1)',
        borderWidth: 5,
        fill: true,
        tension: 0.1,
        },
    ],
};

const chartConfig = {
    type: 'line',
    data: chartData,
    options: {
        scales: {
            x: {
                type: 'time',
                time: {
                },
            }
        }
    },
};

const temperatureChart = new Chart(ctx, chartConfig);

function updateTemperatureChart(timestamp, temperature) {
    chartData.labels.push(timestamp);
    chartData.datasets[0].data.push(temperature);
    temperatureChart.update();
    // Drop data older than 2 weeks or 100 datapoints
    if (chartData.labels.length > 50) {
        chartData.labels.shift();
        chartData.datasets[0].data.shift();
    } else if (chartData.labels.length > 0) {
        const oldestTimestamp = chartData.labels[chartData.labels.length - 1];
        const twoWeeksAgo = Date.now() - 1000 * 60 * 60 * 24 * 14;
        // console.log(new Date(twoWeeksAgo).toLocaleString())
        // console.log("Newest" , new Date(newestTimestamp).toLocaleString())
        // console.log("Oldest" , new Date(oldestTimestamp).toLocaleString())
        // console.log(new Date(Date.now()).toLocaleString())
        if (oldestTimestamp < twoWeeksAgo) {
            chartData.labels.shift();
            chartData.datasets[0].data.shift();
        }
    }
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
                console.log(data);
                console.log(data[0]);
                document.getElementById('air-humidity').innerHTML = Math.floor(data[0].humidity);
                document.getElementById('air-temp').innerHTML = Math.floor(data[0].temperature);
                document.getElementById('timestamp').innerHTML = new Date(data[0].timestamp).toLocaleString();
                
                // Update the chart with the most recent data
                const lastTimestamp = chartData.labels[chartData.labels.length - 1];
                console.log(lastTimestamp)
                // If the chart is not empty, only add new data
                if (lastTimestamp !== undefined) {
                    console.log('not empty')
                    const newData = data.filter(item => {
                        return item.timestamp < lastTimestamp;
                    });
                    newData.forEach((item) => {
                        updateTemperatureChart(item.timestamp, item.temperature);
                    });
                } else {
                    console.log('empty')
                    data.forEach((item) => {
                        updateTemperatureChart(item.timestamp, item.temperature);
                    });
                }
                
            } catch (e) {
                console.log(e);
            }
        })
}

fetchdata();
interval = setInterval(fetchdata, 10000);
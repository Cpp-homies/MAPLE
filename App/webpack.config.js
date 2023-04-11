path = require('path');

module.exports = {
    mode: 'development',
    entry: ['./src/index.js', './src/borders.js'],
    output: {
        filename: 'main.js',
        path: path.resolve(__dirname, 'public'),
    },
    devServer: {
        static: path.join(__dirname, 'public'),
        compress: true,
        port: 80,
        allowedHosts: ['.kovanen.io']
    }
};
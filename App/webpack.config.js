path = require('path');

module.exports = {
    mode: 'development',
    entry: ['./src/index.js', './src/borders.js', './src/auth.js'],
    output: {
        filename: 'main.js',
        path: path.resolve(__dirname, 'public'),
    },
    devServer: {
        static: path.join(__dirname, 'public'),
        compress: true,
        host: '0.0.0.0',
        port: 6900,
        allowedHosts: ['maple.kovanen.io']
    }
};
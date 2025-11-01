from flask import Flask, send_from_directory
from flask_cors import CORS
import os

app = Flask(__name__, static_folder='frontend')
CORS(app) # Enable CORS for all routes, though not strictly needed for static serving

@app.route('/')
def serve_index():
    return send_from_directory(app.static_folder, 'index.html')

@app.route('/<path:path>')
def serve_static(path):
    return send_from_directory(app.static_folder, path)

if __name__ == '__main__':
    app.run(debug=True, port=8000) # Running on port 8000 to avoid conflict with C++ backend (5000)

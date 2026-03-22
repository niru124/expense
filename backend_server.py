import os

from flask import Flask, send_from_directory
from flask_cors import CORS

app = Flask(__name__, static_folder="frontend")
CORS(app)  # Enable CORS for all routes

FRONTEND_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "frontend")


@app.route("/")
def serve_index():
    return send_from_directory(FRONTEND_DIR, "index.html")


@app.route("/index.html")
def serve_app():
    return send_from_directory(FRONTEND_DIR, "index.html")


@app.route("/login")
def serve_login():
    return send_from_directory(FRONTEND_DIR, "login.html")


@app.route("/login.html")
def serve_login_html():
    return send_from_directory(FRONTEND_DIR, "login.html")


@app.route("/register")
def serve_register():
    return send_from_directory(FRONTEND_DIR, "register.html")


@app.route("/register.html")
def serve_register_html():
    return send_from_directory(FRONTEND_DIR, "register.html")


@app.route("/<path:path>")
def serve_static(path):
    return send_from_directory(FRONTEND_DIR, path)


if __name__ == "__main__":
    app.run(
        debug=True, port=8088
    )

from flask import Flask, send_from_directory
import pathlib

app = Flask(__name__, static_folder='www', static_url_path='/')
ROOT = pathlib.Path(__file__).parent / 'www'

@app.route('/')
def index():
    return send_from_directory(ROOT, 'index.html')

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=8000, debug=True)
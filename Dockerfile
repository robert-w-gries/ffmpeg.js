FROM emscripten/emsdk:latest

RUN apt-get update && apt-get install -y git build-essential automake libtool pkg-config && apt-get clean

WORKDIR /app
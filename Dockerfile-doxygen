FROM ubuntu:16.04

RUN apt-get update && apt-get install -y doxygen graphviz

COPY doc /bitcoin-cash-node/doc
COPY src /bitcoin-cash-node/src

WORKDIR /bitcoin-cash-node

RUN doxygen doc/Doxyfile

FROM nginx:alpine

COPY --from=0 /bitcoin-cash-node/doc/doxygen/html /usr/share/nginx/html

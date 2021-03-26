FROM ubuntu:latest

RUN apt-get update && apt-get install -y curl

COPY zflist/zflist /usr/local/bin/
COPY .github/workflows/zflist-publish /usr/local/bin/

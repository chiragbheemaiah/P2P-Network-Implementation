# Use a base image with g++
FROM alpine:latest

RUN apk add --no-cache g++

WORKDIR /app

# Copy your code
COPY . .

# Compile your program
RUN g++ node.cpp -o node

# Run the program
CMD ["tail -f /dev/null"]
#!/bin/bash

echo "=== Repomix Docker Setup ==="
echo "----------------------------"

# Default settings
ACTION="start"
REBUILD=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    stop)
      ACTION="stop"
      shift
      ;;
    restart)
      ACTION="restart"
      shift
      ;;
    logs)
      ACTION="logs"
      shift
      ;;
    --rebuild)
      REBUILD=true
      shift
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: ./docker-start.sh [start|stop|restart|logs] [--rebuild]"
      exit 1
      ;;
  esac
done

# Check if Docker is installed
if ! command -v docker &> /dev/null; then
    echo "❌ Error: Docker not found."
    echo "   Please install Docker first: https://docs.docker.com/get-docker/"
    exit 1
fi

# Check if Docker Compose is installed
if ! command -v docker-compose &> /dev/null; then
    echo "❌ Error: Docker Compose not found."
    echo "   Please install Docker Compose: https://docs.docker.com/compose/install/"
    exit 1
fi

echo "✅ Docker dependencies verified."

# Handle different actions
case $ACTION in
  "start")
    # Build and start the containers
    echo "🚀 Starting Repomix..."
    if [ "$REBUILD" = true ]; then
      echo "Rebuilding containers..."
      docker-compose up --build -d
    else
      docker-compose up -d
    fi

    # Wait for services to be ready
    echo "⏳ Waiting for services to start..."
    sleep 5

    # Check if backend is ready
    echo "⌛ Waiting for backend to be ready..."
    MAX_RETRIES=10
    RETRY_COUNT=0

    while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
        if curl -s http://localhost:9000/api/capabilities > /dev/null; then
            echo "✅ Backend is running on http://localhost:9000"
            break
        fi
        
        RETRY_COUNT=$((RETRY_COUNT+1))
        echo "⏳ Backend not ready yet (attempt $RETRY_COUNT/$MAX_RETRIES). Waiting..."
        sleep 3
        
        if [ $RETRY_COUNT -eq $MAX_RETRIES ]; then
            echo "⚠️ Backend might not be ready yet. Please check docker logs."
        fi
    done

    # Check if the frontend is running
    echo "🔍 Checking frontend service..."
    if curl -s http://localhost:4000 > /dev/null; then
      echo "✅ Frontend is running on http://localhost:4000"
    else
      echo "⚠️  Frontend might not be ready yet. Please check docker logs."
    fi

    echo 
    echo "🌟 Repomix is now running in Docker!"
    echo "📱 Open in your browser: http://localhost:4000"
    ;;
  "stop")
    echo "🛑 Stopping Repomix..."
    docker-compose down
    echo "✅ Repomix stopped"
    ;;
  "restart")
    echo "🔄 Restarting Repomix..."
    if [ "$REBUILD" = true ]; then
      echo "Rebuilding containers..."
      docker-compose down && docker-compose up --build -d
    else
      docker-compose restart
    fi
    echo "✅ Repomix restarted"
    ;;
  "logs")
    echo "📋 Showing logs..."
    docker-compose logs -f
    ;;
esac

echo
echo "📋 Available commands:"
echo "   - Start: ./docker-start.sh"
echo "   - Stop: ./docker-start.sh stop"
echo "   - Restart: ./docker-start.sh restart"
echo "   - View logs: ./docker-start.sh logs"
echo "   - Rebuild and start: ./docker-start.sh --rebuild"
echo 
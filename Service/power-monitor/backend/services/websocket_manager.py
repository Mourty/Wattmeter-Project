from fastapi import WebSocket
from typing import Dict, List
import logging

logger = logging.getLogger(__name__)

class WebSocketManager:
    def __init__(self):
        # Dictionary mapping meter_id to list of connected websockets
        self.active_connections: Dict[str, List[WebSocket]] = {}
    
    async def connect(self, websocket: WebSocket, meter_id: str):
        """Accept a new WebSocket connection"""
        await websocket.accept()
        
        if meter_id not in self.active_connections:
            self.active_connections[meter_id] = []
        
        self.active_connections[meter_id].append(websocket)
        logger.info(f"WebSocket connected for meter {meter_id}")
    
    def disconnect(self, websocket: WebSocket, meter_id: str):
        """Remove a WebSocket connection"""
        if meter_id in self.active_connections:
            if websocket in self.active_connections[meter_id]:
                self.active_connections[meter_id].remove(websocket)
                logger.info(f"WebSocket disconnected for meter {meter_id}")
            
            # Clean up empty lists
            if not self.active_connections[meter_id]:
                del self.active_connections[meter_id]
    
    async def broadcast_to_meter(self, meter_id: str, message: dict):
        """Broadcast a message to all connections for a specific meter"""
        if meter_id in self.active_connections:
            disconnected = []
            
            for connection in self.active_connections[meter_id]:
                try:
                    await connection.send_json(message)
                except Exception as e:
                    logger.error(f"Error sending to websocket: {e}")
                    disconnected.append(connection)
            
            # Remove disconnected clients
            for connection in disconnected:
                self.disconnect(connection, meter_id)
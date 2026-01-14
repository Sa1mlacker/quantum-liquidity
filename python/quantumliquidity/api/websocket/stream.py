"""
WebSocket streaming with MessagePack binary protocol
10x smaller messages, 10x faster parsing
"""

from fastapi import WebSocket, WebSocketDisconnect
import msgpack
import asyncio
import logging
from typing import Dict, Set, Any
from datetime import datetime

logger = logging.getLogger(__name__)


class ConnectionManager:
    """
    WebSocket connection manager
    Handles multiple clients with topic subscriptions
    """

    def __init__(self):
        self.active_connections: Dict[str, WebSocket] = {}
        self.subscriptions: Dict[str, Set[str]] = {}  # client_id -> {topics}
        self.client_counter = 0

    async def connect(self, websocket: WebSocket) -> str:
        """Accept new connection and return client ID"""
        await websocket.accept()
        client_id = f"client_{self.client_counter}"
        self.client_counter += 1

        self.active_connections[client_id] = websocket
        self.subscriptions[client_id] = set()

        logger.info("Client connected: %s (total: %d)",
                    client_id, len(self.active_connections))
        return client_id

    def disconnect(self, client_id: str):
        """Remove client connection"""
        if client_id in self.active_connections:
            del self.active_connections[client_id]
        if client_id in self.subscriptions:
            del self.subscriptions[client_id]

        logger.info("Client disconnected: %s (remaining: %d)",
                    client_id, len(self.active_connections))

    def subscribe(self, client_id: str, topic: str):
        """Subscribe client to topic"""
        if client_id in self.subscriptions:
            self.subscriptions[client_id].add(topic)
            logger.debug("Client %s subscribed to %s", client_id, topic)

    def unsubscribe(self, client_id: str, topic: str):
        """Unsubscribe client from topic"""
        if client_id in self.subscriptions:
            self.subscriptions[client_id].discard(topic)
            logger.debug("Client %s unsubscribed from %s", client_id, topic)

    async def send_personal(self, client_id: str, message: dict):
        """Send message to specific client"""
        if client_id in self.active_connections:
            try:
                data = msgpack.packb(message, use_bin_type=True)
                await self.active_connections[client_id].send_bytes(data)
            except Exception as e:
                logger.error("Failed to send to %s: %s", client_id, e)
                self.disconnect(client_id)

    async def broadcast(self, topic: str, message: dict):
        """Broadcast message to all clients subscribed to topic"""
        # Add metadata
        message["topic"] = topic
        message["timestamp"] = int(datetime.now().timestamp() * 1e9)

        # Binary serialization
        data = msgpack.packb(message, use_bin_type=True)

        # Send to subscribed clients
        disconnected_clients = []

        for client_id, topics in self.subscriptions.items():
            if topic in topics:
                try:
                    websocket = self.active_connections[client_id]
                    await websocket.send_bytes(data)
                except Exception as e:
                    logger.error("Broadcast failed for %s: %s", client_id, e)
                    disconnected_clients.append(client_id)

        # Cleanup disconnected
        for client_id in disconnected_clients:
            self.disconnect(client_id)

    def get_stats(self) -> dict:
        """Get connection statistics"""
        return {
            "total_clients": len(self.active_connections),
            "clients": list(self.active_connections.keys()),
            "subscriptions": {
                client_id: list(topics)
                for client_id, topics in self.subscriptions.items()
            }
        }


# Singleton instance
manager = ConnectionManager()


# ==================== Message Types ====================

class MessageType:
    """WebSocket message types"""
    # Client -> Server
    SUBSCRIBE = "subscribe"
    UNSUBSCRIBE = "unsubscribe"
    PING = "ping"

    # Server -> Client
    PONG = "pong"
    ACK = "ack"
    ERROR = "error"

    # Data streams
    TICK = "tick"
    BAR = "bar"
    POSITION = "position"
    ORDER = "order"
    FILL = "fill"
    RISK = "risk"
    NEWS = "news"


# ==================== Topics ====================

class Topics:
    """Available subscription topics"""

    @staticmethod
    def ticks(instrument: str) -> str:
        return f"ticks:{instrument}"

    @staticmethod
    def bars(instrument: str, timeframe: str) -> str:
        return f"bars:{instrument}:{timeframe}"

    @staticmethod
    def positions() -> str:
        return "positions"

    @staticmethod
    def orders() -> str:
        return "orders"

    @staticmethod
    def fills() -> str:
        return "fills"

    @staticmethod
    def risk() -> str:
        return "risk"

    @staticmethod
    def news(instrument: str = None) -> str:
        if instrument:
            return f"news:{instrument}"
        return "news:all"


# ==================== WebSocket Handler ====================

async def handle_client_message(client_id: str, raw_data: bytes):
    """
    Handle incoming client message
    Messages are MessagePack encoded
    """
    try:
        message = msgpack.unpackb(raw_data, raw=False)
        msg_type = message.get("type")

        if msg_type == MessageType.SUBSCRIBE:
            topic = message.get("topic")
            if topic:
                manager.subscribe(client_id, topic)
                await manager.send_personal(client_id, {
                    "type": MessageType.ACK,
                    "action": "subscribe",
                    "topic": topic
                })

        elif msg_type == MessageType.UNSUBSCRIBE:
            topic = message.get("topic")
            if topic:
                manager.unsubscribe(client_id, topic)
                await manager.send_personal(client_id, {
                    "type": MessageType.ACK,
                    "action": "unsubscribe",
                    "topic": topic
                })

        elif msg_type == MessageType.PING:
            await manager.send_personal(client_id, {
                "type": MessageType.PONG,
                "timestamp": int(datetime.now().timestamp() * 1e9)
            })

        else:
            await manager.send_personal(client_id, {
                "type": MessageType.ERROR,
                "error": f"Unknown message type: {msg_type}"
            })

    except Exception as e:
        logger.error("Failed to handle message from %s: %s", client_id, e)
        await manager.send_personal(client_id, {
            "type": MessageType.ERROR,
            "error": str(e)
        })


async def websocket_endpoint(websocket: WebSocket):
    """
    Main WebSocket endpoint
    Usage:
        ws://localhost:8000/ws/stream

    Message format (binary MessagePack):
        Client -> Server:
            {"type": "subscribe", "topic": "ticks:EUR/USD"}
            {"type": "unsubscribe", "topic": "ticks:EUR/USD"}
            {"type": "ping"}

        Server -> Client:
            {"type": "tick", "topic": "ticks:EUR/USD", "instrument": "EUR/USD",
             "bid": 1.1050, "ask": 1.1051, "timestamp": 1704182400000000000}
    """
    client_id = await manager.connect(websocket)

    try:
        while True:
            # Receive binary message
            data = await websocket.receive_bytes()

            # Handle message
            await handle_client_message(client_id, data)

    except WebSocketDisconnect:
        manager.disconnect(client_id)
    except Exception as e:
        logger.error("WebSocket error for %s: %s", client_id, e)
        manager.disconnect(client_id)


# ==================== Broadcasting Functions ====================

async def broadcast_tick(instrument: str, bid: float, ask: float, **kwargs):
    """Broadcast tick update"""
    topic = Topics.ticks(instrument)
    await manager.broadcast(topic, {
        "type": MessageType.TICK,
        "instrument": instrument,
        "bid": bid,
        "ask": ask,
        **kwargs
    })


async def broadcast_bar(instrument: str, timeframe: str, ohlcv: dict):
    """Broadcast OHLCV bar"""
    topic = Topics.bars(instrument, timeframe)
    await manager.broadcast(topic, {
        "type": MessageType.BAR,
        "instrument": instrument,
        "timeframe": timeframe,
        **ohlcv
    })


async def broadcast_position(position: dict):
    """Broadcast position update"""
    await manager.broadcast(Topics.positions(), {
        "type": MessageType.POSITION,
        **position
    })


async def broadcast_order(order: dict):
    """Broadcast order update"""
    await manager.broadcast(Topics.orders(), {
        "type": MessageType.ORDER,
        **order
    })


async def broadcast_fill(fill: dict):
    """Broadcast fill"""
    await manager.broadcast(Topics.fills(), {
        "type": MessageType.FILL,
        **fill
    })


async def broadcast_risk(risk_metrics: dict):
    """Broadcast risk metrics"""
    await manager.broadcast(Topics.risk(), {
        "type": MessageType.RISK,
        **risk_metrics
    })

"""
Redis caching layer
Aggressive 1-second TTL for desktop app performance
"""

import redis.asyncio as aioredis
import msgpack
import logging
from typing import Any, Optional, Callable
from functools import wraps
import hashlib

from .config import settings

logger = logging.getLogger(__name__)


class CacheService:
    """
    Redis cache with MessagePack serialization
    10x smaller than JSON, 10x faster parsing
    """

    def __init__(self):
        self.redis: Optional[aioredis.Redis] = None
        self.enabled = True

    async def connect(self):
        """Connect to Redis"""
        try:
            self.redis = await aioredis.from_url(
                settings.redis_url,
                encoding="utf-8",
                decode_responses=False,  # Binary mode for msgpack
                max_connections=10,
            )
            # Test connection
            await self.redis.ping()
            logger.info("Redis connected: %s", settings.redis_host)
        except Exception as e:
            logger.error("Redis connection failed: %s", e)
            self.enabled = False

    async def close(self):
        """Close Redis connection"""
        if self.redis:
            await self.redis.close()
            logger.info("Redis connection closed")

    def _serialize(self, data: Any) -> bytes:
        """Serialize data with msgpack"""
        return msgpack.packb(data, use_bin_type=True)

    def _deserialize(self, data: bytes) -> Any:
        """Deserialize data from msgpack"""
        return msgpack.unpackb(data, raw=False)

    def _make_key(self, prefix: str, *args, **kwargs) -> str:
        """Generate cache key from arguments"""
        key_parts = [prefix] + [str(arg) for arg in args]
        if kwargs:
            sorted_kwargs = sorted(kwargs.items())
            key_parts.extend([f"{k}={v}" for k, v in sorted_kwargs])
        return ":".join(key_parts)

    async def get(self, key: str) -> Optional[Any]:
        """Get value from cache"""
        if not self.enabled or not self.redis:
            return None

        try:
            data = await self.redis.get(key)
            if data:
                return self._deserialize(data)
        except Exception as e:
            logger.warning("Cache get failed: %s", e)
        return None

    async def set(
        self,
        key: str,
        value: Any,
        ttl: Optional[int] = None
    ):
        """Set value in cache with TTL"""
        if not self.enabled or not self.redis:
            return

        try:
            data = self._serialize(value)
            if ttl is None:
                ttl = settings.cache_ttl
            await self.redis.setex(key, ttl, data)
        except Exception as e:
            logger.warning("Cache set failed: %s", e)

    async def delete(self, key: str):
        """Delete key from cache"""
        if not self.enabled or not self.redis:
            return

        try:
            await self.redis.delete(key)
        except Exception as e:
            logger.warning("Cache delete failed: %s", e)

    async def delete_pattern(self, pattern: str):
        """Delete all keys matching pattern"""
        if not self.enabled or not self.redis:
            return

        try:
            async for key in self.redis.scan_iter(match=pattern):
                await self.redis.delete(key)
        except Exception as e:
            logger.warning("Cache delete pattern failed: %s", e)

    async def clear_all(self):
        """Clear entire cache (use with caution)"""
        if not self.enabled or not self.redis:
            return

        try:
            await self.redis.flushdb()
            logger.info("Cache cleared")
        except Exception as e:
            logger.warning("Cache clear failed: %s", e)


# Singleton instance
cache = CacheService()


# ==================== Cache Decorator ====================

def cached(
    prefix: str,
    ttl: Optional[int] = None,
    key_builder: Optional[Callable] = None
):
    """
    Decorator for caching function results

    Usage:
        @cached("positions", ttl=1)
        async def get_positions():
            return await db.query(...)

    Args:
        prefix: Cache key prefix
        ttl: Time to live in seconds (default: 1)
        key_builder: Custom function to build cache key
    """
    def decorator(func: Callable):
        @wraps(func)
        async def wrapper(*args, **kwargs):
            # Build cache key
            if key_builder:
                cache_key = key_builder(*args, **kwargs)
            else:
                # Default: prefix + hashed arguments
                arg_str = str(args) + str(sorted(kwargs.items()))
                arg_hash = hashlib.md5(arg_str.encode()).hexdigest()[:8]
                cache_key = f"{prefix}:{arg_hash}"

            # Try cache first
            cached_value = await cache.get(cache_key)
            if cached_value is not None:
                return cached_value

            # Execute function
            result = await func(*args, **kwargs)

            # Cache result
            await cache.set(cache_key, result, ttl=ttl)

            return result

        return wrapper
    return decorator


# ==================== Specialized Cache Keys ====================

class CacheKeys:
    """Standard cache key patterns"""

    @staticmethod
    def positions() -> str:
        return "positions:latest"

    @staticmethod
    def position(instrument: str) -> str:
        return f"position:{instrument}"

    @staticmethod
    def risk_metrics() -> str:
        return "risk:latest"

    @staticmethod
    def orders(status: Optional[str] = None) -> str:
        if status:
            return f"orders:{status}"
        return "orders:all"

    @staticmethod
    def trades(instrument: Optional[str] = None) -> str:
        if instrument:
            return f"trades:{instrument}"
        return "trades:all"

    @staticmethod
    def bars(instrument: str, timeframe: str) -> str:
        return f"bars:{instrument}:{timeframe}"

    @staticmethod
    def daily_stats(instrument: str) -> str:
        return f"stats:daily:{instrument}"

    @staticmethod
    def orb_stats(instrument: str, period: int) -> str:
        return f"stats:orb:{instrument}:{period}"

    @staticmethod
    def sentiment(instrument: str) -> str:
        return f"sentiment:{instrument}"

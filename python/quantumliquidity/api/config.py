"""
API Configuration
Optimized for low resource usage
"""

from pydantic_settings import BaseSettings
from typing import Optional


class Settings(BaseSettings):
    """API Settings from environment variables"""

    # API Config
    api_host: str = "0.0.0.0"
    api_port: int = 8000
    api_workers: int = 1  # Single worker for desktop app
    api_reload: bool = False

    # Database
    db_host: str = "localhost"
    db_port: int = 5432
    db_name: str = "quantumliquidity"
    db_user: str = "postgres"
    db_password: str = "postgres"
    db_pool_size: int = 5  # Small pool for desktop
    db_max_overflow: int = 2

    # Redis Cache
    redis_host: str = "localhost"
    redis_port: int = 6379
    redis_password: Optional[str] = None
    redis_db: int = 0
    cache_ttl: int = 1  # 1 second TTL (aggressive caching)

    # WebSocket
    ws_ping_interval: int = 25  # Seconds
    ws_ping_timeout: int = 60
    ws_max_size: int = 10_485_760  # 10 MB

    # Performance
    max_page_size: int = 100  # Pagination limit
    enable_compression: bool = True  # gzip responses
    enable_msgpack: bool = True  # Binary protocol

    # CORS (for desktop app)
    cors_origins: list[str] = ["tauri://localhost", "http://localhost:1420"]

    # Logging
    log_level: str = "INFO"
    log_json: bool = True  # Structured logs

    @property
    def database_url(self) -> str:
        """Async PostgreSQL connection string"""
        return (
            f"postgresql+asyncpg://{self.db_user}:{self.db_password}"
            f"@{self.db_host}:{self.db_port}/{self.db_name}"
        )

    @property
    def redis_url(self) -> str:
        """Redis connection string"""
        if self.redis_password:
            return f"redis://:{self.redis_password}@{self.redis_host}:{self.redis_port}/{self.redis_db}"
        return f"redis://{self.redis_host}:{self.redis_port}/{self.redis_db}"

    class Config:
        env_file = ".env"
        env_prefix = "QL_"  # QuantumLiquidity prefix


# Singleton instance
settings = Settings()

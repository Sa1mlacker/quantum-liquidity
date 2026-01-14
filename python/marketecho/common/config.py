"""Configuration management"""

from pathlib import Path
from typing import Any
import yaml
from pydantic import BaseModel, Field
from pydantic_settings import BaseSettings


class DatabaseConfig(BaseModel):
    """PostgreSQL database configuration"""
    host: str = "localhost"
    port: int = 5432
    database: str = "quantumliquidity"
    user: str = "quantumliquidity"
    password: str = ""
    pool_size: int = 10
    max_overflow: int = 20


class RedisConfig(BaseModel):
    """Redis configuration"""
    host: str = "localhost"
    port: int = 6379
    db: int = 0
    password: str | None = None
    decode_responses: bool = True


class RiskConfig(BaseModel):
    """Risk management configuration"""
    max_position_value_per_instrument: float = 100000.0
    max_total_exposure: float = 500000.0
    max_daily_loss: float = 10000.0
    max_drawdown_from_peak: float = 50000.0
    max_orders_per_minute: int = 60
    max_leverage: float = 10.0


class Settings(BaseSettings):
    """Application settings"""

    # Environment
    environment: str = Field(default="development", env="ENVIRONMENT")

    # Database
    database: DatabaseConfig = Field(default_factory=DatabaseConfig)

    # Redis
    redis: RedisConfig = Field(default_factory=RedisConfig)

    # Risk
    risk: RiskConfig = Field(default_factory=RiskConfig)

    # Logging
    log_level: str = "INFO"
    log_file: str | None = None

    class Config:
        env_file = ".env"
        env_nested_delimiter = "__"

    @classmethod
    def from_yaml(cls, path: Path) -> "Settings":
        """Load settings from YAML file"""
        with open(path, "r") as f:
            data = yaml.safe_load(f)
        return cls(**data)


# Global settings instance
settings = Settings()

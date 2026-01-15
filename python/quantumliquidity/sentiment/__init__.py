"""
Sentiment Analysis Module
LLM-powered news sentiment for trading signals
"""

from .news_scraper import NewsScraper, NewsArticle
from .sentiment_analyzer import SentimentAnalyzer, SentimentScore
from .sentiment_aggregator import SentimentAggregator

__all__ = [
    'NewsScraper',
    'NewsArticle',
    'SentimentAnalyzer',
    'SentimentScore',
    'SentimentAggregator',
]

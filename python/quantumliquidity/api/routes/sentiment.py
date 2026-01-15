"""Sentiment API routes"""

from fastapi import APIRouter, Depends, Query, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from typing import Optional, List
from datetime import datetime

from ..database import get_db, DatabaseService
from ..cache import cached
from ..schemas import NewsEvent, SentimentSeries
from ...sentiment import NewsScraper, SentimentAnalyzer, SentimentAggregator

router = APIRouter()

# Initialize sentiment components
news_scraper = NewsScraper(max_articles=100)
sentiment_analyzer = SentimentAnalyzer(use_llm=False)  # Set to True for LLM
sentiment_aggregator = SentimentAggregator(news_scraper, sentiment_analyzer)


@router.get("/news", response_model=List[NewsEvent])
@cached("sentiment:news", ttl=10)
async def get_news(
    instrument: Optional[str] = None,
    limit: int = Query(100, le=500),
    db: AsyncSession = Depends(get_db)
):
    """Get recent news events"""
    db_service = DatabaseService(db)
    rows = await db_service.get_news_events(instrument, limit)
    return [NewsEvent(**dict(row._mapping)) for row in rows]


@router.get("/series/{instrument}", response_model=List[SentimentSeries])
@cached("sentiment:series", ttl=5)
async def get_sentiment_series(
    instrument: str,
    limit: int = Query(100, le=500),
    db: AsyncSession = Depends(get_db)
):
    """Get sentiment time series"""
    db_service = DatabaseService(db)
    rows = await db_service.get_sentiment_series(instrument, limit)
    return [SentimentSeries(**dict(row._mapping)) for row in rows]


@router.get("/live/{instrument}")
async def get_live_sentiment(
    instrument: str,
    force_refresh: bool = Query(False, description="Force refresh from sources")
):
    """
    Get live sentiment analysis for instrument

    Returns:
        - Aggregated sentiment score
        - Number of articles analyzed
        - Recent articles
        - Sentiment label (VERY_BULLISH, BULLISH, NEUTRAL, BEARISH, VERY_BEARISH)
    """
    try:
        sentiment = await sentiment_aggregator.get_sentiment(instrument, force_refresh)

        return {
            "instrument": sentiment.instrument,
            "sentiment": {
                "label": sentiment.sentiment.label.value,
                "score": sentiment.sentiment.score,
                "confidence": sentiment.sentiment.confidence,
                "reasoning": sentiment.sentiment.reasoning,
            },
            "num_articles": sentiment.num_articles,
            "last_updated": sentiment.last_updated.isoformat(),
            "recent_articles": [
                {
                    "title": article.title,
                    "source": article.source,
                    "url": article.url,
                    "published_at": article.published_at.isoformat(),
                }
                for article in sentiment.recent_articles
            ],
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/live/multi")
async def get_multi_sentiment(
    instruments: str = Query(..., description="Comma-separated instrument codes"),
    force_refresh: bool = False
):
    """
    Get sentiment for multiple instruments

    Args:
        instruments: Comma-separated (e.g., "ES,NQ,EUR_USD")

    Returns:
        Dict mapping instrument -> sentiment
    """
    instrument_list = [inst.strip() for inst in instruments.split(",")]

    try:
        sentiments = await sentiment_aggregator.get_all_instruments(
            instrument_list, force_refresh
        )

        return {
            instrument: {
                "sentiment": {
                    "label": data.sentiment.label.value,
                    "score": data.sentiment.score,
                    "confidence": data.sentiment.confidence,
                },
                "num_articles": data.num_articles,
                "last_updated": data.last_updated.isoformat(),
            }
            for instrument, data in sentiments.items()
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.post("/analyze")
async def analyze_text(text: str = Query(..., description="Text to analyze")):
    """
    Analyze sentiment of arbitrary text

    Useful for testing or ad-hoc analysis

    Returns:
        SentimentScore with label, score, confidence
    """
    try:
        score = await sentiment_analyzer.analyze(text)

        return {
            "text": text[:100] + "..." if len(text) > 100 else text,
            "sentiment": {
                "label": score.label.value,
                "score": score.score,
                "confidence": score.confidence,
                "reasoning": score.reasoning,
            }
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@router.get("/news/live")
async def get_live_news(
    sources: Optional[str] = Query(None, description="Comma-separated sources"),
    instruments: Optional[str] = Query(None, description="Comma-separated instruments"),
    limit: int = Query(50, le=100)
):
    """
    Fetch live news from sources

    Args:
        sources: reuters,bloomberg,marketwatch,ft (default: all)
        instruments: ES,NQ,EUR_USD, etc. (default: all)
        limit: Max articles to return

    Returns:
        List of recent news articles with sentiment
    """
    try:
        source_list = [s.strip() for s in sources.split(",")] if sources else None
        instrument_list = [i.strip() for i in instruments.split(",")] if instruments else None

        async with news_scraper:
            articles = await news_scraper.fetch_news(source_list, instrument_list)

        # Analyze sentiment for each article
        results = []
        for article in articles[:limit]:
            sentiment_score = await sentiment_analyzer.analyze(article.title)

            results.append({
                "title": article.title,
                "url": article.url,
                "source": article.source,
                "published_at": article.published_at.isoformat(),
                "instruments": article.instruments,
                "sentiment": {
                    "label": sentiment_score.label.value,
                    "score": sentiment_score.score,
                    "confidence": sentiment_score.confidence,
                }
            })

        return {
            "total": len(results),
            "articles": results
        }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

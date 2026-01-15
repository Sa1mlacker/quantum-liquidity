"""
Sentiment Aggregator
Combines news sentiment per instrument over time
"""

from dataclasses import dataclass
from datetime import datetime, timedelta
from typing import Dict, List, Optional
from collections import defaultdict

from .news_scraper import NewsArticle, NewsScraper
from .sentiment_analyzer import SentimentAnalyzer, SentimentScore, SentimentLabel


@dataclass
class InstrumentSentiment:
    """Aggregated sentiment for an instrument"""

    instrument: str
    sentiment: SentimentScore
    num_articles: int
    last_updated: datetime
    recent_articles: List[NewsArticle]


class SentimentAggregator:
    """Aggregates sentiment across multiple news sources with time weighting"""

    def __init__(
        self,
        scraper: NewsScraper,
        analyzer: SentimentAnalyzer,
        lookback_hours: int = 24,
    ):
        self.scraper = scraper
        self.analyzer = analyzer
        self.lookback_hours = lookback_hours

        # Cache
        self.cache: Dict[str, InstrumentSentiment] = {}
        self.last_update: Optional[datetime] = None

    async def get_sentiment(
        self,
        instrument: str,
        force_refresh: bool = False
    ) -> InstrumentSentiment:
        """
        Get current sentiment for an instrument

        Args:
            instrument: Instrument code (e.g., "ES", "EUR_USD")
            force_refresh: Force refresh from news sources

        Returns:
            InstrumentSentiment with aggregated score
        """
        # Check cache (5-minute TTL)
        if not force_refresh and instrument in self.cache:
            cached = self.cache[instrument]
            if (datetime.now() - cached.last_updated).seconds < 300:
                return cached

        # Fetch fresh news
        async with self.scraper:
            articles = await self.scraper.fetch_news(instruments=[instrument])

        if not articles:
            # No news found
            return InstrumentSentiment(
                instrument=instrument,
                sentiment=SentimentScore(
                    label=SentimentLabel.NEUTRAL,
                    score=0.0,
                    confidence=0.0,
                    reasoning="No recent news articles found"
                ),
                num_articles=0,
                last_updated=datetime.now(),
                recent_articles=[],
            )

        # Filter by lookback period
        cutoff_time = datetime.now() - timedelta(hours=self.lookback_hours)
        recent_articles = [
            article for article in articles
            if article.published_at >= cutoff_time
        ]

        # Analyze sentiment for each article
        sentiment_scores = []
        for article in recent_articles:
            score = await self.analyzer.analyze(article.title)
            sentiment_scores.append(score)

        # Time-weighted aggregation (recent news weighted more)
        weighted_scores = []
        now = datetime.now()

        for article, score in zip(recent_articles, sentiment_scores):
            # Calculate time decay (exponential)
            hours_ago = (now - article.published_at).total_seconds() / 3600
            time_weight = 2 ** (-hours_ago / 12)  # Half-life of 12 hours

            # Combine with confidence
            combined_weight = score.confidence * time_weight

            weighted_scores.append(
                SentimentScore(
                    label=score.label,
                    score=score.score,
                    confidence=combined_weight,
                    reasoning=score.reasoning
                )
            )

        # Aggregate
        aggregated = self.analyzer.aggregate_sentiment(weighted_scores)

        result = InstrumentSentiment(
            instrument=instrument,
            sentiment=aggregated,
            num_articles=len(recent_articles),
            last_updated=datetime.now(),
            recent_articles=recent_articles[:5],  # Keep top 5
        )

        # Update cache
        self.cache[instrument] = result
        self.last_update = datetime.now()

        return result

    async def get_all_instruments(
        self,
        instruments: List[str],
        force_refresh: bool = False
    ) -> Dict[str, InstrumentSentiment]:
        """
        Get sentiment for multiple instruments

        Args:
            instruments: List of instrument codes
            force_refresh: Force refresh from sources

        Returns:
            Dict mapping instrument -> InstrumentSentiment
        """
        results = {}

        for instrument in instruments:
            try:
                sentiment = await self.get_sentiment(instrument, force_refresh)
                results[instrument] = sentiment
            except Exception as e:
                print(f"[SentimentAggregator] Error for {instrument}: {e}")

        return results

    def get_sentiment_shift(
        self,
        instrument: str,
        previous: InstrumentSentiment,
        current: InstrumentSentiment
    ) -> Dict[str, any]:
        """
        Detect significant sentiment shifts

        Args:
            instrument: Instrument code
            previous: Previous sentiment
            current: Current sentiment

        Returns:
            Dict with shift analysis
        """
        score_change = current.sentiment.score - previous.sentiment.score
        label_changed = current.sentiment.label != previous.sentiment.label

        # Classify shift magnitude
        if abs(score_change) >= 0.5:
            magnitude = "SIGNIFICANT"
        elif abs(score_change) >= 0.3:
            magnitude = "MODERATE"
        elif abs(score_change) >= 0.1:
            magnitude = "MINOR"
        else:
            magnitude = "NEGLIGIBLE"

        # Direction
        direction = "BULLISH" if score_change > 0 else "BEARISH" if score_change < 0 else "NEUTRAL"

        return {
            "instrument": instrument,
            "magnitude": magnitude,
            "direction": direction,
            "score_change": score_change,
            "label_changed": label_changed,
            "previous_score": previous.sentiment.score,
            "current_score": current.sentiment.score,
            "previous_label": previous.sentiment.label.value,
            "current_label": current.sentiment.label.value,
            "alert": magnitude in ["SIGNIFICANT", "MODERATE"],
        }

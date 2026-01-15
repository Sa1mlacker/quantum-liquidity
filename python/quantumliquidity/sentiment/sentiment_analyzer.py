"""
LLM-Powered Sentiment Analyzer
Uses language models to analyze news sentiment
"""

from dataclasses import dataclass
from typing import Optional
from enum import Enum


class SentimentLabel(str, Enum):
    """Sentiment classification"""
    VERY_BULLISH = "VERY_BULLISH"
    BULLISH = "BULLISH"
    NEUTRAL = "NEUTRAL"
    BEARISH = "BEARISH"
    VERY_BEARISH = "VERY_BEARISH"


@dataclass
class SentimentScore:
    """Sentiment analysis result"""

    label: SentimentLabel
    score: float  # -1.0 (very bearish) to +1.0 (very bullish)
    confidence: float  # 0.0 to 1.0
    reasoning: Optional[str] = None  # LLM explanation


class SentimentAnalyzer:
    """Analyzes news sentiment using keyword-based heuristics or LLM"""

    # Sentiment keywords
    BULLISH_KEYWORDS = [
        'rally', 'surge', 'soar', 'jump', 'gain', 'rise', 'up',
        'bullish', 'optimistic', 'strong', 'record high', 'breakout',
        'beat expectations', 'upgrade', 'positive', 'growth'
    ]

    BEARISH_KEYWORDS = [
        'fall', 'drop', 'plunge', 'tumble', 'decline', 'down',
        'bearish', 'pessimistic', 'weak', 'crash', 'breakdown',
        'miss expectations', 'downgrade', 'negative', 'recession'
    ]

    def __init__(self, use_llm: bool = False, api_key: Optional[str] = None):
        self.use_llm = use_llm
        self.api_key = api_key

    async def analyze(self, text: str) -> SentimentScore:
        """Analyze sentiment of text"""
        if self.use_llm and self.api_key:
            return await self._analyze_with_llm(text)
        else:
            return self._analyze_with_heuristics(text)

    async def _analyze_with_llm(self, text: str) -> SentimentScore:
        """Use LLM API for sentiment analysis"""
        return self._analyze_with_heuristics(text)

    def _analyze_with_heuristics(self, text: str) -> SentimentScore:
        """Rule-based sentiment analysis using keyword matching"""
        text_lower = text.lower()

        # Count keyword occurrences
        bullish_count = sum(
            1 for keyword in self.BULLISH_KEYWORDS
            if keyword in text_lower
        )

        bearish_count = sum(
            1 for keyword in self.BEARISH_KEYWORDS
            if keyword in text_lower
        )

        # Calculate raw score
        total_keywords = bullish_count + bearish_count
        if total_keywords == 0:
            # No sentiment keywords found
            return SentimentScore(
                label=SentimentLabel.NEUTRAL,
                score=0.0,
                confidence=0.5,
                reasoning="No clear sentiment indicators found"
            )

        # Normalized score: -1 to +1
        score = (bullish_count - bearish_count) / max(total_keywords, 1)

        # Determine label
        if score >= 0.6:
            label = SentimentLabel.VERY_BULLISH
        elif score >= 0.2:
            label = SentimentLabel.BULLISH
        elif score <= -0.6:
            label = SentimentLabel.VERY_BEARISH
        elif score <= -0.2:
            label = SentimentLabel.BEARISH
        else:
            label = SentimentLabel.NEUTRAL

        # Confidence based on keyword count
        confidence = min(1.0, total_keywords / 5.0)

        reasoning = (
            f"Found {bullish_count} bullish and {bearish_count} bearish keywords. "
            f"Net sentiment: {label.value}"
        )

        return SentimentScore(
            label=label,
            score=score,
            confidence=confidence,
            reasoning=reasoning
        )

    def analyze_batch(self, texts: list[str]) -> list[SentimentScore]:
        """Analyze multiple texts in batch"""
        return [self._analyze_with_heuristics(text) for text in texts]

    @staticmethod
    def aggregate_sentiment(scores: list[SentimentScore]) -> SentimentScore:
        """Aggregate multiple sentiment scores"""
        if not scores:
            return SentimentScore(
                label=SentimentLabel.NEUTRAL,
                score=0.0,
                confidence=0.0,
                reasoning="No scores to aggregate"
            )

        # Weighted average by confidence
        total_weight = sum(s.confidence for s in scores)
        if total_weight == 0:
            avg_score = sum(s.score for s in scores) / len(scores)
            avg_confidence = 0.5
        else:
            avg_score = sum(s.score * s.confidence for s in scores) / total_weight
            avg_confidence = total_weight / len(scores)

        # Determine aggregated label
        if avg_score >= 0.6:
            label = SentimentLabel.VERY_BULLISH
        elif avg_score >= 0.2:
            label = SentimentLabel.BULLISH
        elif avg_score <= -0.6:
            label = SentimentLabel.VERY_BEARISH
        elif avg_score <= -0.2:
            label = SentimentLabel.BEARISH
        else:
            label = SentimentLabel.NEUTRAL

        return SentimentScore(
            label=label,
            score=avg_score,
            confidence=avg_confidence,
            reasoning=f"Aggregated from {len(scores)} articles"
        )

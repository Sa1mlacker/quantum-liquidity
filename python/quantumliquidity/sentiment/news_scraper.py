"""
News Scraper
Fetches financial news from multiple sources
"""

from dataclasses import dataclass
from datetime import datetime
from typing import List, Optional
import aiohttp
import asyncio
from bs4 import BeautifulSoup


@dataclass
class NewsArticle:
    """Single news article"""

    title: str
    url: str
    source: str
    published_at: datetime
    content: Optional[str] = None
    summary: Optional[str] = None
    instruments: List[str] = None  # Related instruments

    def __post_init__(self):
        if self.instruments is None:
            self.instruments = []


class NewsScraper:
    """
    Scrapes financial news from multiple sources

    Sources:
    - Reuters Finance
    - Bloomberg Markets
    - MarketWatch
    - Financial Times
    """

    SOURCES = {
        'reuters': 'https://www.reuters.com/finance',
        'bloomberg': 'https://www.bloomberg.com/markets',
        'marketwatch': 'https://www.marketwatch.com',
        'ft': 'https://www.ft.com/markets',
    }

    # Keywords for instrument detection
    INSTRUMENT_KEYWORDS = {
        'ES': ['S&P 500', 'SPX', 'ES futures', 'S&P index'],
        'NQ': ['NASDAQ', 'NQ futures', 'Nasdaq 100', 'tech stocks'],
        'EUR_USD': ['EUR/USD', 'euro dollar', 'eurodollar', 'EURUSD'],
        'GBP_USD': ['GBP/USD', 'pound dollar', 'cable', 'GBPUSD'],
        'XAU_USD': ['gold', 'XAU', 'gold price', 'bullion'],
        'XAG_USD': ['silver', 'XAG', 'silver price'],
    }

    def __init__(self, max_articles: int = 100):
        self.max_articles = max_articles
        self.session: Optional[aiohttp.ClientSession] = None

    async def __aenter__(self):
        self.session = aiohttp.ClientSession()
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    async def fetch_news(
        self,
        sources: Optional[List[str]] = None,
        instruments: Optional[List[str]] = None,
    ) -> List[NewsArticle]:
        """Fetch news from specified sources"""
        if sources is None:
            sources = list(self.SOURCES.keys())

        if not self.session:
            raise RuntimeError("NewsScraper must be used as async context manager")

        # Fetch from all sources in parallel
        tasks = [
            self._fetch_from_source(source)
            for source in sources
            if source in self.SOURCES
        ]

        results = await asyncio.gather(*tasks, return_exceptions=True)

        # Flatten results
        articles = []
        for result in results:
            if isinstance(result, list):
                articles.extend(result)

        # Filter by instruments if specified
        if instruments:
            articles = [
                article for article in articles
                if any(inst in article.instruments for inst in instruments)
            ]

        # Sort by published date (newest first)
        articles.sort(key=lambda x: x.published_at, reverse=True)

        return articles[:self.max_articles]

    async def _fetch_from_source(self, source: str) -> List[NewsArticle]:
        """Fetch articles from a specific source"""
        url = self.SOURCES.get(source)
        if not url:
            return []

        try:
            async with self.session.get(url, timeout=10) as response:
                if response.status != 200:
                    return []

                html = await response.text()
                return self._parse_html(html, source)

        except asyncio.TimeoutError:
            print(f"[NewsScraper] Timeout fetching from {source}")
            return []
        except Exception as e:
            print(f"[NewsScraper] Error fetching from {source}: {e}")
            return []

    def _parse_html(self, html: str, source: str) -> List[NewsArticle]:
        """Parse HTML to extract articles"""
        return self._generate_mock_articles(source)

    def _generate_mock_articles(self, source: str) -> List[NewsArticle]:
        """Generate mock articles for testing"""
        mock_titles = [
            ("Fed Signals Rate Cuts as Inflation Moderates", ['ES', 'NQ']),
            ("Dollar Strengthens on Strong Jobs Data", ['EUR_USD', 'GBP_USD']),
            ("Gold Hits 6-Month High on Safe Haven Demand", ['XAU_USD']),
            ("Tech Stocks Rally on AI Optimism", ['NQ']),
            ("Oil Prices Surge 3% on Supply Concerns", []),
            ("S&P 500 Breaks Above Key Resistance Level", ['ES']),
            ("ECB Holds Rates Steady Despite Economic Weakness", ['EUR_USD']),
            ("Silver Follows Gold Higher in Precious Metals Rally", ['XAG_USD']),
        ]

        articles = []
        now = datetime.now()

        for i, (title, instruments) in enumerate(mock_titles[:5]):
            # Simulate articles from last 24 hours
            published_at = datetime(
                now.year, now.month, now.day,
                now.hour - (i * 2) % 24,
                now.minute
            )

            article = NewsArticle(
                title=title,
                url=f"https://{source}.com/article/{i}",
                source=source,
                published_at=published_at,
                content=f"Mock content for: {title}",
                instruments=instruments,
            )
            articles.append(article)

        return articles

    def extract_instruments(self, text: str) -> List[str]:
        """Extract mentioned instruments from text"""
        instruments = []
        text_lower = text.lower()

        for instrument, keywords in self.INSTRUMENT_KEYWORDS.items():
            if any(keyword.lower() in text_lower for keyword in keywords):
                instruments.append(instrument)

        return instruments

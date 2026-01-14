# QuantumLiquidity: Quick Start Guide

## Для нетерплячих (5 хвилин)

```bash
# 1. Перейти в директорію проєкту
cd /Users/lacker/Downloads/IT/Trading/gtrade-echo

# 2. Запустити PostgreSQL та Redis
make docker-up

# 3. Перевірити, що сервіси запущені
docker-compose ps

# 4. Створити конфігурацію
cp config/example.yaml config/local.yaml
cp .env.example .env

# 5. (Опціонально) Відкрити Adminer для перегляду БД
# http://localhost:8080
# Server: postgres, User: quantumliquidity, Password: changeme, Database: quantumliquidity
```

Готово! Інфраструктура запущена.

## Що Далі?

### Варіант A: Дослідити архітектуру

1. Прочитати [ARCHITECTURE.md](ARCHITECTURE.md) — розуміння системи
2. Переглянути [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) — що де лежить
3. Вивчити [database/schema.sql](database/schema.sql) — структура даних

### Варіант B: Почати програмувати

1. Прочитати [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)
2. Обрати Phase 1 (Foundation)
3. Почати з `cpp/src/persistence/postgres_writer.cpp`

### Варіант C: Запустити щось

**Поки що нічого не запуститься** — потрібна імплементація. Але можна:

```bash
# Спробувати build C++ (fail, бо не всі бібліотеки)
cd cpp
mkdir build && cd build
cmake ..

# Встановити Python пакет
cd ../../python
pip install -e .

# Імпортувати в Python (працює!)
python -c "from quantumliquidity.common import settings; print(settings)"
```

## Що Маємо

✅ Повну архітектуру (Production-grade)  
✅ Структуру проєкту (C++ + Python)  
✅ Database schema (PostgreSQL/TimescaleDB)  
✅ Конфігураційну систему  
✅ Docker infrastructure  
✅ Build system (CMake + pyproject.toml)  
✅ Документацію (4 MD файли)  

## Що Потрібно

❌ Імплементація C++ core (market data, execution, risk)  
❌ Імплементація Python сервісів (analytics, sentiment)  
❌ Provider integrations (брокери)  
❌ LLM integration для sentiment  
❌ Tests  

## Оцінка Часу до Робочого MVP

**8-10 тижнів** full-time розробки (1 людина)

**Або**:
- 4-5 тижнів з командою з 2 людей
- 2-3 тижні з досвідченою командою з 3-4 людей

## Критичний Шлях (Priority 1)

1. Week 1: Database layer + Logging
2. Week 2: Market Data Gateway (CSV provider для тестів)
3. Week 3-4: Risk Engine + Execution (Mock broker)
4. Week 5: Strategy Engine + ORB strategy
5. Week 6-7: Analytics + Historical Data
6. Week 8: Testing + Backtests

**Sentiment Engine** — паралельно або Phase 2.

## Залежності для Встановлення

### C++ Libraries (vcpkg recommended)

```bash
# Install vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh

# Install dependencies
./vcpkg install libpq hiredis protobuf spdlog yaml-cpp nlohmann-json gtest
```

### Python (вже в pyproject.toml)

```bash
cd python
pip install -e ".[dev,ai]"
```

## Корисні Команди

```bash
# Запустити PostgreSQL shell
make db-shell

# Скинути базу даних
make db-reset

# Зупинити інфраструктуру
make docker-down

# Очистити все (включно з volumes)
make docker-clean
```

## Структура Документації

Читати в такому порядку:

1. [README.md](README.md) — загальний огляд
2. [QUICK_START.md](QUICK_START.md) — ця сторінка
3. [ARCHITECTURE.md](ARCHITECTURE.md) — детальна архітектура
4. [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) — що де знаходиться
5. [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md) — покрокова імплементація
6. [SUMMARY.md](SUMMARY.md) — що зроблено, що треба

## Питання?

Див. [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md) секцію "Questions to Resolve".

## Disclaimer

Це production-grade архітектура, але **skeleton implementation**.  
Усі TODO місця явно позначені.  
Не очікуйте, що щось запуститься без додаткової роботи.

**Готові до розробки? → [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)**

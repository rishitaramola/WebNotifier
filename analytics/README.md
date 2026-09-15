# Analytics Engine Module
**Owner:** Rishita Ramola

## Purpose
Queries historical monitoring data from PostgreSQL, computes uptime
statistics, and exports weekly reports as CSV and PDF.

## Files
| File | Description |
|---|---|
| `include/analytics_engine.h` | AnalyticsEngine class declaration |
| `src/analytics_engine.cpp` | Aggregation queries and report export |

## DBMS Concepts Demonstrated
- **Aggregation**: `COUNT`, `SUM`, `AVG`, `ROUND`, `GROUP BY`
- **Views**: uses `v_uptime_last7days` pre-computed view
- **Date filtering**: `WHERE checked_at >= NOW() - INTERVAL N days`

## Output Formats
| Format | Method | Notes |
|---|---|---|
| CSV | `export_csv()` | Comma-separated, importable to Excel |
| PDF | `export_pdf()` | TODO: wkhtmltopdf integration |
| Console | `print_summary()` | Quick terminal summary |

## Sample Report Query
```sql
SELECT w.url,
       COUNT(*) AS checks,
       ROUND(100.0 * SUM(CASE WHEN status='"'"'UP'"'"' THEN 1 ELSE 0 END)/COUNT(*), 2) AS uptime_pct,
       AVG(response_time_ms) AS avg_ms
FROM monitoring_results mr
JOIN websites w ON mr.website_id = w.id
WHERE w.user_id = 1
  AND mr.checked_at >= NOW() - INTERVAL '"'"'7 days'"'"'
GROUP BY w.url;
```

CREATE TABLE TEST(id INTEGER, part TEXT, val INTEGER);

INSERT INTO TEST
VALUES
  (1, 'A', 1),
  (2, 'A', NULL),
  (3, 'A', 3),
  (4, 'B', NULL),
  (5, 'B', 5),
  (6, 'B', NULL),
  (7, 'B', 7);

SELECT id, LAST_NON_NULL(val) OVER (PARTITION BY part ORDER BY id ASC) AS val
FROM TEST
ORDER BY id ASC;

SELECT 1 AS one;

SELECT name, count(*) FROM items GROUP BY name;

UPDATE items SET name = 'two' WHERE id = 2;

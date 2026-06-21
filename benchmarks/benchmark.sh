#!/bin/bash
# MiniDB Benchmark Script
# Tests insert, select, and delete performance

echo "=== MiniDB Benchmark ==="
echo ""

MINIDB="./minidb"

if [ ! -f "$MINIDB" ]; then
    echo "Error: minidb executable not found. Run 'make' first."
    exit 1
fi

# Clean up previous data
rm -rf minidb_data

echo "--- Test 1: Table Creation ---"
echo "CREATE TABLE bench (id INT, value TEXT, score INT)" | $MINIDB 2>/dev/null | tail -1

echo ""
echo "--- Test 2: Bulk Insert (100 rows) ---"
START=$(date +%s%N)
for i in $(seq 1 100); do
    echo "INSERT INTO bench VALUES ($i, 'item_$i', $((RANDOM % 100)))"
done | $MINIDB 2>/dev/null | tail -1
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))
echo "Time: ${ELAPSED}ms for 100 inserts"

echo ""
echo "--- Test 3: Full Table Scan ---"
START=$(date +%s%N)
echo "SELECT * FROM bench" | $MINIDB 2>/dev/null | tail -1
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))
echo "Time: ${ELAPSED}ms"

echo ""
echo "--- Test 4: Filtered Select (WHERE) ---"
START=$(date +%s%N)
echo "SELECT * FROM bench WHERE id = 50" | $MINIDB 2>/dev/null | tail -1
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))
echo "Time: ${ELAPSED}ms"

echo ""
echo "--- Test 5: Delete with WHERE ---"
START=$(date +%s%N)
echo "DELETE FROM bench WHERE id = 50" | $MINIDB 2>/dev/null | tail -1
END=$(date +%s%N)
ELAPSED=$(( (END - START) / 1000000 ))
echo "Time: ${ELAPSED}ms"

echo ""
echo "=== Benchmark Complete ==="

# Clean up
rm -rf minidb_data

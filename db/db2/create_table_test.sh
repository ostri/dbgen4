#!/bin/bash

# Enable debugging
#set -x

# Attempt to connect to the database 'test'
db2 connect to test

# Check if connection was successful
if [ $? -ne 0 ]; then
  echo "Failed to connect to database 'test'. Please check credentials or database configuration."
  exit 1
fi

# Check if the table 'test' already exists
CHECK_TABLE=$(db2 -x "SELECT COUNT(*) FROM syscat.tables WHERE tabname = 'TEST' AND tabschema = CURRENT SCHEMA")

if [ "$CHECK_TABLE" -gt 0 ]; then
  echo "Table 'test' already exists. Do you want to drop it? (yes/no)"
  read confirm_drop
  if [ "$confirm_drop" != "yes" ]; then
    echo "Table drop not confirmed. Exiting."
    db2 terminate
    exit 1
  else
    # Drop the table
    db2 -v "DROP TABLE test"
    echo "Table 'test' dropped."
  fi
fi

# Create the table using here document - ADDED BINARY and VARBINARY
db2 -tv << EOF
CREATE TABLE test (
  col_SMALLINT SMALLINT,
  col_INTEGER INTEGER,
  col_BIGINT BIGINT,
  col_DECIMAL DECIMAL(10,2),
  col_REAL REAL,
  col_DOUBLE DOUBLE,
  col_DECFLOAT DECFLOAT,
  col_CHAR CHAR(2),
  col_VARCHAR VARCHAR(255),
  col_CLOB CLOB(1K),
  col_GRAPHIC GRAPHIC(2),
  col_VARGRAPHIC VARGRAPHIC(255),
  col_DBCLOB DBCLOB(1K),
  col_BLOB BLOB(1K),
  col_BINARY BINARY(8),       -- Dodan BINARY stolpec
  col_VARBINARY VARBINARY(16), -- Dodan VARBINARY stolpec
  col_DATE DATE,
  col_TIME TIME,
  col_TIMESTAMP TIMESTAMP,
  col_BOOLEAN BOOLEAN,
  col_XML XML
);
EOF

# Independently check if the table was created
CHECK=$(db2 -x "SELECT COUNT(*) FROM syscat.tables WHERE tabname = 'TEST' AND tabschema = CURRENT SCHEMA")

if [ "$CHECK" -gt 0 ]; then
  echo "Table 'test' was successfully created."
else
  echo "Table 'test' was not created."
  db2 terminate
  exit 1
fi

# Ask user for the number of records to insert
echo "How many records would you like to insert?"
read num_records

# Validate input
if ! [[ "$num_records" =~ ^[0-9]+$ ]] || [ "$num_records" -lt 1 ]; then
  echo "Please enter a valid positive number."
  db2 terminate
  exit 1
fi

# Loop to insert the specified number of records
for ((i=1; i<=num_records; i++))
do
  # Numeric values will be based on the record number
  smallint_val=$i
  integer_val=$i
  bigint_val=$i
  decimal_val=$(echo "$i + 0.1 * $i" | bc)
  real_val=$(echo "$i + 0.01 * $i" | bc)
  double_val=$(echo "$i + 0.001 * $i" | bc)
  decfloat_val=$i

  # Non-numeric values will include the record number for identification
  char_val=$(echo "A$((i % 26))" | tr '0-9' 'A-J')
  varchar_val="Record_$i"
  clob_val="CLOB_data_for_record_$i"
  graphic_val=$(echo "G$((i % 26))" | tr '0-9' 'A-J')
  vargraphic_val="Vargraphic_$i"
  dbclob_val="DBCLOB_data_for_record_$i"

  # BLOB, BINARY, VARBINARY: Simple hex string based on record number
  # BLOB: 8 bajtov za DEADBEEF + 4 bajti za številko zapisa (skupaj 12 bajtov - 24 hex znakov)
  blob_val=$(printf "DEADBEEF%08X" $i)

  # BINARY: 8 bajtov (16 hex znakov). Zapolnimo z vodilnimi ničlami.
  binary_val=$(printf "%016X" $i)

  # VARBINARY: Uporabimo 4 bajte (8 hex znakov).
  varbinary_val=$(printf "BEEF%04X" $i)


  # Date, Time, Timestamp
  date_val="2025-10-27"
  time_val=$(printf "12:%02d:00" $((i % 60)))
  timestamp_val=$(printf "2025-10-27 12:%02d:00.000000" $((i % 60)))

  # Boolean: Alternate between true and false
  boolean_val=$([ $((i % 2)) -eq 0 ] && echo "TRUE" || echo "FALSE")

  # XML: Simple XML string with record number
  xml_val="<record id=\"$i\"><data>Sample XML for record $i</data></record>"

  # Construct and execute the INSERT statement - ADDED BINARY and VARBINARY
  db2 -tv << EOF
INSERT INTO test (
  col_SMALLINT, col_INTEGER, col_BIGINT, col_DECIMAL, col_REAL, col_DOUBLE, col_DECFLOAT,
  col_CHAR, col_VARCHAR, col_CLOB, col_GRAPHIC, col_VARGRAPHIC, col_DBCLOB, col_BLOB,
  col_BINARY, col_VARBINARY,
  col_DATE, col_TIME, col_TIMESTAMP, col_BOOLEAN, col_XML
) VALUES (
  $smallint_val,
  $integer_val,
  $bigint_val,
  $decimal_val,
  $real_val,
  $double_val,
  $decfloat_val,
  '$char_val',
  '$varchar_val',
  '$clob_val',
  '$graphic_val',
  '$vargraphic_val',
  '$dbclob_val',
  CAST(X'$blob_val' AS BLOB),
  CAST(X'$binary_val' AS BINARY(8)),       -- Vstavljanje BINARY
  CAST(X'$varbinary_val' AS VARBINARY(16)), -- Vstavljanje VARBINARY
  '$date_val',
  '$time_val',
  '$timestamp_val',
  $boolean_val,
  XMLPARSE(DOCUMENT '$xml_val')
);
EOF

  # Check if the insert was successful
  if [ $? -eq 0 ]; then
    echo "Record $i inserted successfully."
  else
    echo "Failed to insert record $i."
  fi
done

# Verify the number of inserted records
INSERTED_COUNT=$(db2 -x "SELECT COUNT(*) FROM test")

echo "Total records in table 'test': $INSERTED_COUNT"

# Terminate the connection
db2 terminate
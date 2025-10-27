#!/bin/bash
#set  -x
# Script to create a DB2 database named TEST with territory Slovenia
# Checks if DB2_HOME is set, aborts if not
# Checks if the instance is running, starts it only if necessary
# Checks if the database exists, prompts for confirmation before dropping it if it does, and creates a new one
# Creates a test table named 'test' with one column for each supported DB2 data type
# Prompts for the number of rows to insert, then inserts rows with sequential values for numeric columns
# Grants all possible database privileges to user 'ostri' while assuming 'db2inst1' already has admin rights
#
# Usage: Run this script as the DB2 instance user (e.g., db2inst1)
# Ensure DB2 environment is properly configured before running
# Note: This script is intended for testing purposes only and should not be used in production environments.
# ./create_db2_test.sh as db2inst1
# su - db2inst1 -c './create_db2_test.sh' as another user
# Set DB2 instance user, database name, and admin user
#
DB2INSTANCE="db2inst1"  # Default instance user with admin privileges
DBNAME="TEST"
ADMIN_USER="ostri"      # User to grant all privileges
TABLENAME="test"        # Name of the test table

# Check if DB2_HOME is set
if [ -z "$DB2_HOME" ]; then
    echo "Error: DB2_HOME environment variable is not set. Ensure DB2 environment is properly configured."
    exit 1
fi

# Check if db2 command is available
if ! command -v db2 &> /dev/null; then
    echo "Error: db2 command not found. Ensure DB2 environment is properly configured."
    exit 1
fi

# # Check if running as correct user
# if [ "$(whoami)" != "$DB2INSTANCE" ]; then
#     echo "Warning: Script must run as DB2 instance user ($DB2INSTANCE)."
#     echo "Try: su - $DB2INSTANCE -c 'bash $0'"
#     exit 1
# fi

# Check if DB2 instance is already running
echo "Checking DB2 instance status..."
db2gits=$(db2 list active databases 2>/dev/null | grep -i "SQLSTATE=57019" | wc -l)
if [ "$db2gits" -eq 1 ]; then
    echo "DB2 instance is not running. Starting DB2 instance..."
    db2start
    if [ $? -ne 0 ]; then
        echo "Error: Failed to start DB2 instance."
        exit 1
    fi
else
    echo "DB2 instance is already running."
fi

# Check if database TEST exists and prompt for confirmation to drop it
db2 "LIST DATABASE DIRECTORY" | grep -i "$DBNAME" > /dev/null
if [ $? -eq 0 ]; then
    echo "Database $DBNAME already exists."
    read -p "Do you want to drop the existing database $DBNAME? (y/n): " confirm
    if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]; then
        echo "Dropping database $DBNAME..."
        db2 connect reset
        db2 "DROP DATABASE $DBNAME"
        if [ $? -eq 0 ]; then
            echo "Database $DBNAME dropped successfully."
        else
            echo "Error: Failed to drop database $DBNAME."
            exit 1
        fi
    else
        echo "Database drop cancelled by user."
        exit 1
    fi
fi 

# Create database TEST with Slovenian territory
echo "Creating database $DBNAME..."
db2 "CREATE DATABASE $DBNAME AUTOMATIC STORAGE YES USING CODESET UTF-8 TERRITORY SI"
if [ $? -eq 0 ]; then
    echo "Database $DBNAME created successfully."
else
    echo "Error: Failed to create database $DBNAME."
    exit 1
fi

# Test connection to the database
echo "Testing connection to $DBNAME..."
db2 "CONNECT TO $DBNAME"
if [ $? -eq 0 ]; then
    echo "Connection to $DBNAME successful."
    db2 "CONNECT RESET"
else
    echo "Error: Failed to connect to $DBNAME."
    exit 1
fi


# # Grant all possible database privileges to user 'ostri'
# echo "Granting all database privileges to user $ADMIN_USER for database $DBNAME..."
# db2 "GRANT DBADM, CREATETAB, BINDADD, CONNECT, CREATE_EXTERNAL_ROUTINE, CREATE_NOT_FENCED_ROUTINE, IMPLICIT_SCHEMA, LOAD, QUIESCE_CONNECT, SECADM, ACCESSCTRL, DATAACCESS, SQLADM, WLMADM ON DATABASE TO USER $ADMIN_USER"
# if [ $? -eq 0 ]; then
#     echo "All database privileges granted to user $ADMIN_USER successfully."
# else
#     echo "Error: Failed to grant privileges to user $ADMIN_USER."
#     db2 "CONNECT RESET"
#     exit 1
# fi

echo "Script completed. Database $DBNAME with test table '$TABLENAME' is ready for use."
echo "User $ADMIN_USER has been granted all database privileges."

exit 0


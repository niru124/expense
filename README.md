# Finance CLI Application

This is a C++ command-line interface (CLI) application for managing personal finance, built with the Crow C++ web framework for API exposure and SQLite3 for data storage.

## How to Run

To compile and run this application, follow these steps:

### Prerequisites

Before compiling, ensure you have the necessary development libraries installed on your system:

*   **SQLite3 Development Libraries:**
    On Debian/Ubuntu-based systems, install with:
    ```bash
    sudo apt-get update
    sudo apt-get install libsqlite3-dev
    ```
    On Arch-based systems, install with:
    ```bash
    sudo pacman -S sqlite
    ```
    For other Linux distributions, please refer to your package manager's documentation for installing SQLite3 development headers.

*   **Crow C++ Web Framework:** Crow is primarily header-only, but its dependencies (like Boost) might need to be installed. Ensure you have Crow properly set up or included in your project.

*   **Boost Libraries (System and Thread):**
    On Debian/Ubuntu-based systems, install with:
    ```bash
    sudo apt-get install libboost-system-dev libboost-thread-dev
    ```
    On Arch-based systems, install with:
    ```bash
    sudo pacman -S boost
    ```

### Compilation and Execution

1.  Navigate to the project root directory in your terminal.
2.  Make the `run.sh` script executable:
    ```bash
    chmod +x run.sh
    ```
3.  Compile the project:
    ```bash
    cmake . && make
    ```
4.  Run the application:
    ```bash
    make run
    ```

    This will compile the `main.cpp`, `FinanceDB.cpp`, and `helper.cpp` files, link the necessary libraries, and then execute the compiled application. The application will start a web server, typically on `http://localhost:5000`.

## API Endpoints

The application exposes the following API endpoints:

### 1. Home Route
*   **URL:** `/`
*   **Method:** `GET`
*   **Description:** Provides a welcome message and links to other main endpoints.
*   **Response:** HTML content.

### 2. Get All Monthly Summaries
*   **URL:** `/summary`
*   **Method:** `GET`
*   **Description:** Retrieves a list of all recorded monthly summaries, including salary, limit, saving percentage, and condition.
*   **Response:** JSON array of monthly summary objects.
    ```json
    [
        {
            "month_year": "08_2025",
            "salary": 5000.0,
            "limit": 4000.0,
            "saving_percentage": 20.0,
            "condition": "Good"
        }
    ]
    ```

### 3. Get Expenses for a Specific Month
*   **URL:** `/expenses/<month_year>` (e.g., `/expenses/08_2025`)
*   **Method:** `GET`
*   **Description:** Retrieves a list of detailed expense records for a specified month and year.
*   **Response:** JSON array of expense record objects.
    ```json
    [
        {
            "day_month_year": "2025-08-15_12345",
            "spent_on": "Groceries",
            "price": 150.75,
            "priority": 5
        }
    ]
    ```

### 4. Add or Update Monthly Summary
*   **URL:** `/summary`
*   **Method:** `POST`
*   **Description:** Adds a new monthly summary or updates an existing one for the current month. Requires `salary` and `limit`.
*   **Request Body:** JSON object.
    ```json
    {
        "salary": 5000.0,
        "limit": 4000.0
    }
    ```
*   **Response:** Success or error message.

### 5. Add New Expense
*   **URL:** `/expense`
*   **Method:** `POST`
*   **Description:** Adds a new expense record for the current day. Automatically updates the priority of the `spentOn` item and recalculates the monthly summary if a salary is set.
*   **Request Body:** JSON object.
    ```json
    {
        "spentOn": "Dinner",
        "price": 75.50
    }
    ```
*   **Response:** Success or error message.

### 6. Get Prioritized Expenses (Highest Frequency/Average Price)
*   **URL:** `/highest`
*   **Method:** `GET`
*   **Description:** Retrieves a list of expenses grouped by `spent_on`, showing the average price and the number of times purchased (priority), sorted by purchase frequency in descending order.
*   **Response:** JSON array of prioritized expense objects.
    ```json
    [
        {
            "spent_on": "Coffee",
            "average_price": 4.50,
            "times_purchased": 10
        }
    ]
    ```

### 7. Get Expenses Sorted by Price
*   **URL:** `/sorted_by_price/<order>` (e.g., `/sorted_by_price/true` for ascending, `/sorted_by_price/false` for descending)
*   **URL:** `/sorted_by_price/` (defaults to ascending)
*   **Method:** `GET`
*   **Description:** Retrieves all expense records for the current month, sorted by price in either ascending or descending order.
*   **Response:** JSON array of expense record objects.
    ```json
    [
        {
            "day_month_year": "2025-08-10_98765",
            "spent_on": "Bus Fare",
            "price": 2.50,
            "priority": 3
        }
    ]
    ```

### 8. Get Expenses within a Date Range
*   **URL:** `/range/<start_date>/<end_date>` (e.g., `/range/01-08-2025/15-08-2025`)
*   **Method:** `GET`
*   **Description:** Retrieves expense records within a specified date range (inclusive). Dates should be in `DD-MM-YYYY` format.
*   **Response:** JSON array of expense record objects.
    ```json
    [
        {
            "day_month_year": "2025-08-05_54321",
            "spent_on": "Lunch",
            "price": 12.00,
            "priority": 2
        }
    ]
    ```

### 9. Get Total Spent
*   **URL:** `/total_spent`
*   **Method:** `GET`
*   **Description:** Retrieves the total amount spent for the current month.
*   **Response:** JSON object with the total amount.
    ```json
    {
        "total": 1500.50
    }
    ```

### 10. Delete Expense by ID
*   **URL:** `/delete_expense/<id>` (e.g., `/delete_expense/123`)
*   **Method:** `DELETE`
*   **Description:** Deletes an expense record by its unique ID.
*   **Response:** Success or error message.
    ```json
    "Expense with ID 123 deleted successfully."
    ```
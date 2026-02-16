# Finance Tracker Application

This project consists of a C++ backend for managing personal finance data (using Crow C++ web framework and SQLite3) and a simple HTML/Tailwind CSS/JavaScript frontend for interaction.

## Project Structure

```
expense/
├── CMakeLists.txt          # CMake build configuration
├── README.md               # This file
├── study.md               # Study notes
├── todo.md                # Todo list
├── .gitignore             # Git ignore rules
├── backend_server.py      # Python Flask server for frontend
├── run.sh                # Quick start script (in scripts/)
├── sciplot/              # Third-party plotting library (header-only)
├── frontend/             # HTML/Tailwind CSS frontend
│   └── index.html
├── src/                  # C++ source files
│   ├── main.cpp          # Main entry point and API routes
│   ├── FinanceDB.cpp     # Database operations
│   └── helper.cpp        # Helper utilities
├── include/              # Header files
│   ├── FinanceDB.h       # Database class declarations
│   └── helper.h          # Helper function declarations
└── build/                # Build output directory (created by CMake)
```

## Directory Descriptions

- **`src/`**: Contains all C++ source files (.cpp). This is where the main application logic lives.
- **`include/`**: Contains all C++ header files (.h) for class declarations and function prototypes.
- **`frontend/`**: Contains the static HTML frontend with Tailwind CSS via CDN and JavaScript for API calls.
- **`scripts/`**: Contains utility scripts like `run.sh` for building and running the application.
- **`sciplot/`**: Third-party header-only library for generating expense graphs.

## How to Run the Application

To get the entire application (C++ backend and HTML frontend) up and running, follow these steps:

### Prerequisites

Before compiling and running, ensure you have the necessary tools and libraries installed on your system:

*   **CMake:** For building the C++ backend.
    On Arch Linux, install with: `sudo pacman -S cmake`
*   **g++ (C++ Compiler):** A C++17 compatible compiler.
*   **SQLite3 Development Libraries:**
    On Debian/Ubuntu-based systems: `sudo apt-get update && sudo apt-get install libsqlite3-dev`
    On Arch Linux: `sudo pacman -S sqlite`
*   **Crow C++ Web Framework:** Crow is primarily header-only. Ensure its dependencies are met. (The `CMakeLists.txt` handles finding it).
*   **Python 3 and Flask:** For the frontend server.
    Install Flask: `pip install Flask`

### Running the Application

1.  **Navigate to the project root directory** in your terminal.

2.  **Make the `run.sh` script executable** (if you haven't already):
    ```bash
    chmod +x scripts/run.sh
    ```

3.  **Execute the `run.sh` script:**
    ```bash
    ./scripts/run.sh
    ```
    This script will:
    *   Build the C++ backend using CMake.
    *   Start the C++ backend server (on `http://localhost:5000`) in the background.
    *   Start the Python frontend server (on `http://localhost:8000`) in the background.
    *   Provide instructions on how to access the frontend.

4.  **Access the Frontend:** Open your web browser and navigate to `http://localhost:8000`.

    *To stop both servers, simply press `Ctrl+C` in the terminal where `run.sh` is running.*

### Manual Build and Run

If you prefer to build and run manually:

1.  **Create and navigate to build directory:**
    ```bash
    mkdir -p build && cd build
    ```

2.  **Configure with CMake:**
    ```bash
    cmake ..
    ```

3.  **Build the project:**
    ```bash
    cmake --build .
    ```

4.  **Run the C++ backend:**
    ```bash
    ./expense &
    ```

5.  **Start the Python frontend server** (from project root):
    ```bash
    python3 backend_server.py &
    ```

## C++ Backend API Endpoints

The C++ backend (running on `http://localhost:5000`) exposes the following API endpoints:

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
            "month_year": "11_2025",
            "salary": 5000.0,
            "limit": 4000.0,
            "saving_percentage": 20.0,
            "condition": "Good"
        }
    ]
    ```

### 3. Get Expenses for a Specific Month
*   **URL:** `/expenses/<month_year>` (e.g., `/expenses/11_2025`)
*   **Method:** `GET`
*   **Description:** Retrieves a list of detailed expense records for a specified month and year.
*   **Response:** JSON array of expense record objects, now including `id` (SQLite `rowid`).
    ```json
    [
        {
            "id": 1,
            "day_month_year": "01_11_2025_12345",
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
*   **Response:** Success or error message (text).

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
*   **Response:** Success or error message (text).

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
*   **Description:** Retrieves all expense records for the current month, sorted by price in either ascending or descending order. Now includes `id` (SQLite `rowid`).
*   **Response:** JSON array of expense record objects.
    ```json
    [
        {
            "id": 2,
            "day_month_year": "01_11_2025_98765",
            "spent_on": "Bus Fare",
            "price": 2.50,
            "priority": 3
        }
    ]
    ```

### 8. Get Expenses within a Date Range
*   **URL:** `/range/<start_date>/<end_date>` (e.g., `/range/01-11-2025/15-11-2025`)
*   **Method:** `GET`
*   **Description:** Retrieves expense records within a specified date range (inclusive). Dates should be in `DD-MM-YYYY` format. Now includes `id` (SQLite `rowid`).
*   **Response:** JSON array of expense record objects.
    ```json
    [
        {
            "id": 3,
            "day_month_year": "05_11_2025_54321",
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
*   **Description:** Deletes an expense record by its unique ID (SQLite `rowid`).
*   **Response:** Success or error message (text).
    ```
    Expense with ID 123 deleted successfully.
    ```

### 11. Edit Expense by ID
*   **URL:** `/edit_expense/<id>` (e.g., `/edit_expense/123`)
*   **Method:** `PUT`
*   **Description:** Updates an existing expense record by its unique ID (SQLite `rowid`). Accepts optional `spentOn`, `price`, and `priority` fields.
*   **Request Body:** JSON object (at least one field required).
    ```json
    {
        "spentOn": "New Description",
        "price": 25.00,
        "priority": 4
    }
    ```
*   **Response:** Success or error message (text).
    ```
    Expense with ID 123 updated successfully.
    ```

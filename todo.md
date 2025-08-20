Here are some suggested new endpoints:

- [x] `/expenses/sorted_by_price` (GET): List expenses by price. (Uses getSortedByVal())
- [ ] `/expenses/category/<string>` (GET): List expenses by category. (Needs getExpensesByCategory())
- [ ] `/expenses/date/<YYYY-MM-DD>` (GET): List expenses for a specific date. (Needs getExpensesForDate())
- [ ] `/expenses/range/<start_date>/<end_date>` (GET): List expenses within a date range. (Needs getExpensesForDateRange())
- [ ] `/total_spent` (GET): Total spent for current month. (Needs getTotalSpentForCurrentMonth())
- [ ] `/delete_expense/<string>` (DELETE): Delete expense by ID. (Needs deleteExpense())
- [ ] `/edit_expense/<string>` (PUT): Edit expense by ID. (Needs editExpense())

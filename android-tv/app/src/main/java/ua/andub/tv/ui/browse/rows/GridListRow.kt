package ua.andub.tv.ui.browse.rows

import androidx.leanback.widget.HeaderItem
import androidx.leanback.widget.ListRow
import androidx.leanback.widget.ObjectAdapter

class GridListRow(
    headerItem: HeaderItem,
    adapter: ObjectAdapter,
    val numRows: Int = 1
) : ListRow(headerItem, adapter)

package ua.andub.tv.ui.browse.presenters

import androidx.leanback.widget.ListRowPresenter
import androidx.leanback.widget.RowPresenter
import ua.andub.tv.ui.browse.rows.GridListRow

class GridListRowPresenter : ListRowPresenter() {

    init {
        shadowEnabled = false
        selectEffectEnabled = false
    }

    override fun initializeRowViewHolder(holder: RowPresenter.ViewHolder) {
        super.initializeRowViewHolder(holder)
        val gridRow = holder.row as? GridListRow
        val numRows = gridRow?.numRows ?: 1
        (holder as? ViewHolder)?.gridView?.setNumRows(numRows)
    }
}

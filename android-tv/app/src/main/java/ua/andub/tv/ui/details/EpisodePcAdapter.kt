package ua.andub.tv.ui.details

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import ua.andub.tv.R
import ua.andub.tv.data.models.Episode

class EpisodePcAdapter(
    private var episodes: List<Episode>,
    private val isWatched: (String) -> Boolean,
    private val onItemClick: (Episode) -> Unit
) : RecyclerView.Adapter<EpisodePcAdapter.ViewHolder>() {

    fun updateEpisodes(newEpisodes: List<Episode>) {
        episodes = newEpisodes
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_episode_pc_style, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val ep = episodes[position]
        holder.bind(ep)
    }

    override fun getItemCount(): Int = episodes.size

    inner class ViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val titleView: TextView = itemView.findViewById(R.id.tv_episode_title)
        private val watchedIcon: ImageView = itemView.findViewById(R.id.iv_watched_indicator)
        private val watchBtn: TextView = itemView.findViewById(R.id.tv_watch_button)

        init {
            itemView.setOnClickListener {
                val pos = bindingAdapterPosition
                if (pos != RecyclerView.NO_POSITION && pos < episodes.size) {
                    onItemClick(episodes[pos])
                }
            }

            itemView.setOnFocusChangeListener { v, hasFocus ->
                if (hasFocus) {
                    v.animate().scaleX(1.01f).scaleY(1.02f).setDuration(120).start()
                    titleView.setTextColor(0xFFFFFFFF.toInt())
                    watchBtn.setBackgroundColor(0xFF38BDF8.toInt())
                    watchBtn.setTextColor(0xFF0B0C10.toInt())
                } else {
                    v.animate().scaleX(1.0f).scaleY(1.0f).setDuration(120).start()
                    titleView.setTextColor(0xFFF1F5F9.toInt())
                    watchBtn.setBackgroundResource(R.drawable.bg_episode_watch_btn)
                    watchBtn.setTextColor(0xFFFFFFFF.toInt())
                }
            }
        }

        fun bind(episode: Episode) {
            val rawTitle = episode.title.replace("— null", "").replace("- null", "").trim()
            titleView.text = if (rawTitle.isNotBlank() && rawTitle != "null") rawTitle else "Серія ${episode.number}"

            val watched = isWatched(episode.number)
            watchedIcon.visibility = if (watched) View.VISIBLE else View.GONE
        }
    }
}

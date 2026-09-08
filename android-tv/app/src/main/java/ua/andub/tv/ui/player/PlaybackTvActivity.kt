package ua.andub.tv.ui.player

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.WindowManager
import androidx.fragment.app.FragmentActivity
import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode

class PlaybackTvActivity : FragmentActivity() {

    companion object {
        fun start(context: Context, anime: Anime, episode: Episode, episodes: List<Episode>) {
            val intent = Intent(context, PlaybackTvActivity::class.java).apply {
                putExtra(PlaybackTvFragment.EXTRA_ANIME, anime)
                putExtra(PlaybackTvFragment.EXTRA_EPISODE, episode)
                putExtra(PlaybackTvFragment.EXTRA_ALL_EPISODES, ArrayList(episodes))
            }
            context.startActivity(intent)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        if (savedInstanceState == null) {
            val anime = intent.getSerializableExtra(PlaybackTvFragment.EXTRA_ANIME) as? Anime ?: return
            val episode = intent.getSerializableExtra(PlaybackTvFragment.EXTRA_EPISODE) as? Episode ?: return
            @Suppress("UNCHECKED_CAST")
            val episodes = (intent.getSerializableExtra(PlaybackTvFragment.EXTRA_ALL_EPISODES) as? ArrayList<Episode>) ?: emptyList()

            supportFragmentManager.beginTransaction()
                .replace(android.R.id.content, PlaybackTvFragment.newInstance(anime, episode, episodes))
                .commit()
        }
    }
}

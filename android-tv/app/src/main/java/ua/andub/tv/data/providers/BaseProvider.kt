package ua.andub.tv.data.providers

import ua.andub.tv.data.models.Anime
import ua.andub.tv.data.models.Episode
import ua.andub.tv.data.models.Stream

abstract class BaseProvider {
    abstract val name: String
    open val displayName: String
        get() = name

    abstract suspend fun search(
        query: String = "",
        limit: Int = 30,
        genre: String = "",
        page: Int = 1
    ): List<Anime>

    abstract suspend fun getEpisodes(anime: Anime): List<Episode>

    abstract suspend fun getStream(anime: Anime, episode: Episode): Stream
}

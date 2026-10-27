cmake_minimum_required(VERSION 3.5)
project(unsplash-wallpapers)

set(BACKGROUNDS
	default.jpg
	wallpaper-1.jpg
	wallpaper-2.jpg
	wallpaper-3.jpg
	wallpaper-4.jpg
	wallpaper-5.jpg
	wallpaper-6.jpg
	wallpaper-7.jpg
	wallpaper-8.jpg
	wallpaper-9.jpg
)

install(FILES ${BACKGROUNDS} DESTINATION /usr/share/backgrounds/bayuos)
# Sets of a picture for the morning, the day, the evening and the night; the
# Appearance service shows the one for the time of day or the theme.
install(DIRECTORY dynamic DESTINATION /usr/share/backgrounds/bayuos)

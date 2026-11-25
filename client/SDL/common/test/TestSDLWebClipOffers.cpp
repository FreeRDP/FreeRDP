#include <tuple>

#include "../../SDL3/sdl_web_clip_offers.hpp"

int TestSDLWebClipOffers(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	const std::string local = "copied in the browser";
	const std::string remote = "copied in the session";

	{
		SdlWebClipOffers offers;
		if (!offers.offer(local, nullptr, 0))
			return 1;
		/* Without a server clipboard in between, offering the same data again is harmless. */
		if (!offers.offer(local, nullptr, 0))
			return 2;
	}

	{
		/* The server copies files, which we don't fetch, or its text is still on the way. */
		SdlWebClipOffers offers;
		std::ignore = offers.offer(local, nullptr, 0);
		offers.serverAnnounced();
		if (offers.offer(local, nullptr, 0))
			return 3;
		/* A new local copy still takes the clipboard back. */
		if (!offers.offer("copied again in the browser", nullptr, 0))
			return 4;
	}

	{
		/* The server's text reached the browser clipboard and is offered back on focus. */
		SdlWebClipOffers offers;
		std::ignore = offers.offer(local, nullptr, 0);
		offers.serverAnnounced();
		offers.serverSentText(remote);
		if (offers.offer(remote, nullptr, 0))
			return 5;
		/* The page replaced our old offer, so copying it again is a new local copy. */
		if (!offers.offer(local, nullptr, 0))
			return 6;
	}

	{
		/* An image offered with the server's text is new content. */
		SdlWebClipOffers offers;
		const BYTE image[] = { 1, 2, 3 };
		offers.serverAnnounced();
		offers.serverSentText(remote);
		if (!offers.offer(remote, image, sizeof(image)))
			return 7;
	}

	{
		SdlWebClipOffers offers;
		const BYTE image[] = { 1, 2, 3 };
		std::ignore = offers.offer("", image, sizeof(image));
		offers.serverAnnounced();
		offers.serverSentImage();
		if (!offers.offer("", image, sizeof(image)))
			return 8;
	}

	return 0;
}

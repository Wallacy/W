// HTTP component entry. The host owns sockets, TLS, and request admission.

import { fetch } from restaurant.gateway

entry LastLightWorker(fetch)

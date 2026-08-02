// HTTP component entry. The host owns sockets, TLS, and request admission.

import { fetch } from gateway

entry LastLightWorker(fetch)

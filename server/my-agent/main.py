import os
import asyncio

import tornado.web
import tornado.httpserver

from conf import LoadConf, Conf, IsLocalhost


cwd = os.getcwd()
print('Working Directory:', cwd)

# Load conf file
LoadConf(os.getenv("CONFIG_FILE", "conf.dev.json" if IsLocalhost else "conf.prod.json"))


class IndexHandler(tornado.web.RequestHandler):
    async def get(self):
        self.write("hi")


class NotFoundHandler(tornado.web.RequestHandler):
    def response_404(self):
        self.set_status(404)
        self.write({"code": 404, "path": self.request.path})

    def get(self):
        self.response_404()

    def post(self):
        self.response_404()


def create_app() -> tornado.web.Application:
    import req_iot
    import req_tools
    routes: list = []
    routes += req_iot.ROUTES
    routes += req_tools.ROUTES
    routes += [
        (r"/", IndexHandler),
    ]
    return tornado.web.Application(
        routes,
        debug=IsLocalhost,
        autoreload=False,
        default_handler_class=NotFoundHandler
    )


async def serve_forever():
    app = create_app()
    server = tornado.httpserver.HTTPServer(app, xheaders=True)
    port = int(os.getenv('PORT', str(Conf.port)))
    server.listen(port, address='0.0.0.0')  # IPv4
    server.listen(port, address='::')  # IPv6

    print(f"Server is running on port {port}")

    shutdown_event = asyncio.Event()
    await shutdown_event.wait()


if __name__ == "__main__":
    asyncio.run(serve_forever())

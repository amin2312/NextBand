import json

import tornado.web
from pydantic import BaseModel, ValidationError
from typing import TypeVar, Type

T = TypeVar('T', bound=BaseModel)


class HTTPError(Exception):
    def __init__(self, name: str, message: str | None = None, status_code: int = 500):
        self.status_code = status_code
        self.detail = None
        if not self.detail:
            self.detail = {"name": name, "error": message}
        super().__init__(self.detail)

    def to_json(self):
        return json.dumps(self.detail)


class CrosHandler(tornado.web.RequestHandler):
    def set_default_headers(self):
        self.set_header("Access-Control-Allow-Origin", "*")
        self.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
        self.set_header("Access-Control-Allow-Headers", "*")

    def options(self, *args, **kwargs):
        self.set_status(204)
        self.finish()


class ParseHandler(tornado.web.RequestHandler):
    def prepare(self):
        print(self.request.method, self.request.path, )

    def parse_body(self, model: Type[T]) -> T:
        body = self.request.body or b'{}'
        try:
            return model.model_validate_json(body)
        except ValidationError as e:
            raise HTTPError("INVALID_PARAM", str(e), 400)

    def parse_args(self, model: Type[T]) -> T:
        body = {
            k: v[-1].decode() for k, v in self.request.query_arguments.items()
        }
        try:
            return model.model_validate(body)
        except ValidationError as e:
            raise HTTPError("INVALID_PARAM", str(e), 400)

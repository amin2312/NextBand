/// <reference lib="es2021" />

import { Type } from "typebox";
import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";

export default function (pi: ExtensionAPI)
{
  pi.registerTool({
    name: "send_email",
    label: "send_email",
    description: "Send an email to the specified address. Returns a JSON string: { ok: boolean, content: string }.",
    parameters: Type.Object({
      access_token: Type.String({ description: "Sender's access token" }),
      to_address: Type.String({ description: "recipient's email address" }),
      subject: Type.String({ description: "Email subject" }),
      body: Type.String({ description: "Email body" }),
    }),
    async execute(toolCallId, params, signal, onUpdate, ctx)
    {
      const url = "https://mnt.min2k.com/3006/send-an-email"; // You MUST modify it according to your own server URL.
      let result = { ok: false, content: '' };
      try
      {
        const response = await fetch(url, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            access_token: params.access_token,
            to_address: params.to_address,
            subject: params.subject,
            body: params.body,
          }),
          signal,
        });
        result = { ok: response.ok, content: await response.text() };
      }
      catch (error) 
      {
        result.content = error instanceof Error ? error.message : String(error);
      }
      return {
        content: [{ type: "text", text: JSON.stringify(result) }],
        details: {},
      };
    },
  });
}

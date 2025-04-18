/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! Uses an Asynchronous HTTP client to send a `POST` request with multipart body.

use ylong_http_client::async_impl::{Client, Downloader, MultiPart, Part, Uploader};
use ylong_http_client::{Method, Request};

#[tokio::main]
async fn main() {
    // Customizes your HTTP client.
    let client = Client::builder().build().unwrap();

    // Customize your `Multipart` messages.
    let multipart = MultiPart::new()
        .part(Part::new().name("name").body("xiaoming")) // Adds your parts.
        .part(Part::new().name("password").body("123456789"))
        .part(
            Part::new()
                .name("123")
                .length(Some(10))
                .stream("HelloWorld".as_bytes()),
        );

    // Uses `Uploader` to upload the `Multipart` with progress message displayed on console.
    let uploader = Uploader::builder().multipart(multipart).console().build();

    // Customizes your HTTP request.
    let request = Request::builder()
        .method(Method::POST)
        .url("http://www.example.com")
        .multipart(uploader) // Sets the multipart body.
        .unwrap();

    // Sends your HTTP request through the client.
    let response = client.request(request).await.unwrap();

    // Uses `Downloader` to download the response body and display message on console.
    let _ = Downloader::console(response).download().await;
}

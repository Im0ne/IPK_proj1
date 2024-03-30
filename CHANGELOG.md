# Changelog

All notable changes to this project will be documented in this file.

## 2024-02-17

- Start of the project

## 2024-03-01

- Command line parser and hostname to IPv4 convertion

## 2024-03-12

- Main functionality of TCP
  - Client and server messages parsing in TCP

## 2024-03-14

- Fixed some checks of validity of messages from server

## 2024-03-16

- Start of UDP
  - Handle user commands, creation of some messages, connection to server

- Code refactoring

## 2024-03-17

- Progress in UDP
  - Completed message creation
  - Added server replies handling
  - Added waiting for confirmation from server

## 2024-03-19

- Fixes in TCP
_ UDP changes
  - Created function to transition to error state
  - Minor code formatting

## 2024-03-28

- TCP, now it can proccess couple of messages in one packet + controls of messages
- UDP fixes
  - Proper graceful exit
  - Receiving packets from any port now
  - Check of server messages
  - Fixed bug when first message is ignored

## 2024-03-30

- Added documentation, changelog, license, examples of usage/testing, and mock server

## Known Limitations

When user is typing message and there is message from server it will be printed over this message but only visually, really there is two different streams, it can be fixed but it is over assignment.

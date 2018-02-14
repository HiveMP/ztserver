using IO.Swagger.Api;
using IO.Swagger.Client;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace TestZt
{
    class Program
    {
        static int Main(string[] args)
        {
            var processA = Process.Start("ztserver.exe", "-h 8080");
            var processB = Process.Start("ztserver.exe", "-h 8081 -z 9996");

            Thread.Sleep(3000);

            var clientSocketA = new SocketApi("http://localhost:8080");
            var clientSocketB = new SocketApi("http://localhost:8081");
            var clientNetworkA = new NetworkApi("http://localhost:8080");
            var clientNetworkB = new NetworkApi("http://localhost:8081");

            Console.WriteLine("Joining networks...");

            clientNetworkA.Join(new IO.Swagger.Model.ZeroTierJoinRequest
            {
                Nwid = "8bd5124fd6206336",
                Path = "ztidentityA",
            });
            clientNetworkB.Join(new IO.Swagger.Model.ZeroTierJoinRequest
            {
                Nwid = "8bd5124fd6206336",
                Path = "ztidentityB",
            });

            Console.WriteLine("Creating local UDP ports...");

            var udpClientA = new UdpClient(0, AddressFamily.InterNetwork);
            var udpClientB = new UdpClient(0, AddressFamily.InterNetwork);

            var portA = ((IPEndPoint)udpClientA.Client.LocalEndPoint).Port;
            var portB = ((IPEndPoint)udpClientB.Client.LocalEndPoint).Port;

            Console.WriteLine("Forwarding ports...");

            var forwardA = clientSocketA.Forward(new IO.Swagger.Model.ZeroTierForwardRequest(portA));
            var forwardB = clientSocketB.Forward(new IO.Swagger.Model.ZeroTierForwardRequest(portB));

            try
            {
                Console.WriteLine("Retrieving address information...");

                var infoA = clientNetworkA.GetInfo();
                var infoB = clientNetworkB.GetInfo();

                Console.WriteLine("Sending packet from client A to client B...");

                var helloWorld = "Hello World";
                var msg = new List<byte>();
                msg.Add(0);
                msg.AddRange(IPAddress.Parse(infoB.Addresses.First(x => x.Contains("."))).GetAddressBytes());
                msg.AddRange(BitConverter.GetBytes((ushort)forwardB.Zt4port.Value).Reverse() /* endian switch LE -> NBO */);
                msg.AddRange(Encoding.ASCII.GetBytes(helloWorld));
                var msgBytes = msg.ToArray();
                udpClientA.Send(msgBytes, msgBytes.Length, new IPEndPoint(IPAddress.Loopback, forwardA.Proxyport.Value));

                Console.WriteLine("Trying to receive a packet on client B from client A...");
                IPEndPoint r = new IPEndPoint(IPAddress.Loopback, 0);
                var recvMsg = udpClientB.Receive(ref r);
                if (recvMsg == null)
                {
                    Console.WriteLine("Failed to receive message!");
                    return 1;
                }

                if (recvMsg.Length < 10)
                {
                    Console.WriteLine("Received message was unexpected length.");
                    return 1;
                }

                if (recvMsg[0] == 1)
                {
                    Console.WriteLine("Message didn't have IPv4 header");
                    return 1;
                }

                var recvAddr = new IPAddress(new[]
                {
                    recvMsg[1],
                    recvMsg[2],
                    recvMsg[3],
                    recvMsg[4],
                });
                var recvPort = BitConverter.ToUInt16(new[]
                {
                    recvMsg[6],
                    recvMsg[5],
                }, 0);

                var clientAAddress = IPAddress.Parse(infoA.Addresses.First(x => x.Contains(".")));

                if (!recvAddr.Equals(clientAAddress))
                {
                    Console.WriteLine("Message didn't arrive from client A IP address");
                    return 1;
                }
                if (recvPort != (ushort)forwardA.Zt4port.Value)
                {
                    Console.WriteLine("Message didn't arrive from expected port");
                    return 1;
                }

                Console.WriteLine("TEST PASS - Received message over ZT between two clients!");

                return 0;
            }
            finally
            {
                Console.WriteLine("Unforwarding ports...");

                clientSocketA.Unforward(new IO.Swagger.Model.ZeroTierUnforwardRequest
                {
                    Proxyport = forwardA.Proxyport
                });
                clientSocketB.Unforward(new IO.Swagger.Model.ZeroTierUnforwardRequest
                {
                    Proxyport = forwardB.Proxyport
                });

                Console.WriteLine("Leaving networks...");

                clientNetworkA.Leave(new IO.Swagger.Model.ZeroTierLeaveRequest());
                clientNetworkB.Leave(new IO.Swagger.Model.ZeroTierLeaveRequest());

                processA.Kill();
                processB.Kill();
            }
        }
    }
}
